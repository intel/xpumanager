//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"context"
	"sync"
	"time"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.opentelemetry.io/collector/pdata/plog"
	"go.uber.org/zap"

	l0sysman "github.com/intel/level-zero-go/sysman"
	"github.com/intel/xpumanager/xpumd/common"
	"github.com/intel/xpumanager/xpumd/receiver/sysman/internal/metadata"
)

// allEventTypeFlags is the bitmask of all known Sysman event types.
const allEventTypeFlags = l0sysman.EventTypeFlags(
	l0sysman.EVENT_TYPE_FLAG_DEVICE_DETACH |
		l0sysman.EVENT_TYPE_FLAG_DEVICE_ATTACH |
		l0sysman.EVENT_TYPE_FLAG_DEVICE_SLEEP_STATE_ENTER |
		l0sysman.EVENT_TYPE_FLAG_DEVICE_SLEEP_STATE_EXIT |
		l0sysman.EVENT_TYPE_FLAG_FREQ_THROTTLED |
		l0sysman.EVENT_TYPE_FLAG_ENERGY_THRESHOLD_CROSSED |
		l0sysman.EVENT_TYPE_FLAG_TEMP_CRITICAL |
		l0sysman.EVENT_TYPE_FLAG_TEMP_THRESHOLD1 |
		l0sysman.EVENT_TYPE_FLAG_TEMP_THRESHOLD2 |
		l0sysman.EVENT_TYPE_FLAG_MEM_HEALTH |
		l0sysman.EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH |
		l0sysman.EVENT_TYPE_FLAG_PCI_LINK_HEALTH |
		l0sysman.EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS |
		l0sysman.EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS |
		l0sysman.EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED |
		l0sysman.EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED,
)

type sysmanEventsReceiver struct {
	drivers  []*driver
	consumer consumer.Logs
	logger   *zap.SugaredLogger
	wg       sync.WaitGroup
	stop     context.CancelFunc
}

func newSysmanEventsReceiver(devices *deviceRegistry, logger *zap.SugaredLogger, nextConsumer consumer.Logs) (*sysmanEventsReceiver, error) {
	r := &sysmanEventsReceiver{
		drivers:  devices.drivers,
		consumer: nextConsumer,
		logger:   logger,
	}

	// Register for all event types on every device.
	for i, drv := range r.drivers {
		registered := 0
		for j, dev := range drv.devices {
			if err := dev.EventRegister(allEventTypeFlags); err != nil {
				r.logger.Errorw("Device EventRegister() failed: device events unavailable",
					zap.Error(err), "deviceID", j+1, "deviceAttributes", dev.attributes)
				continue
			}
			registered++
		}
		logger.Infow("Sysman devices registered for events", "registered", registered, "enumerated", len(drv.devices), "driverIndex", i)
	}

	return r, nil
}

// Start implements collector component.Component.Start.
func (r *sysmanEventsReceiver) Start(ctx context.Context, _ component.Host) error {
	ctx, r.stop = context.WithCancel(ctx)
	for _, drv := range r.drivers {
		r.wg.Go(func() {
			r.runEventListener(ctx, drv)
		})
	}
	return nil
}

// Shutdown implements collector component.Component.Shutdown.
func (r *sysmanEventsReceiver) Shutdown(_ context.Context) error {
	if r.stop != nil {
		r.stop()
	}
	r.wg.Wait()
	return nil
}

// listenTimeout is the maximum time to block in EventListenEx, effectively a
// poll interval when there are no events.
const listenTimeout = 1 * time.Second

func (r *sysmanEventsReceiver) runEventListener(ctx context.Context, drv *driver) {
	l0Devices := make([]*l0sysman.Device, len(drv.devices))
	for i, d := range drv.devices {
		l0Devices[i] = d.Device
	}

	var prevZeroTime time.Time
	for {
		select {
		case <-ctx.Done():
			return
		default:
		}

		deviceCount, deviceEvents, err := drv.driver.EventListenEx(listenTimeout, l0Devices)
		// TODO: revisit error handling, bail out on all errors except perhaps
		// RESULT_ERROR_DEVICE_LOST, rescan drivers and devices on some others(?)
		if err != nil {
			r.logger.Warnw("EventListenEx failed", zap.Error(err))
			// Brief back-off to avoid flooding on persistent errors.
			select {
			case <-time.After(5 * time.Second):
			case <-ctx.Done():
				return
			}
			continue
		}
		if deviceCount == 0 {
			if time.Since(prevZeroTime) < listenTimeout/2 {
				// Avoid busy loop on consecutive zero-event returns
				time.Sleep(listenTimeout)
			}
			prevZeroTime = time.Now()
			continue
		}

		ld := plog.NewLogs()
		rl := ld.ResourceLogs().AppendEmpty()
		sl := rl.ScopeLogs().AppendEmpty()
		sl.Scope().SetName(metadata.ScopeName)

		ts := pcommon.NewTimestampFromTime(time.Now())

		for i, flags := range deviceEvents {
			if flags != 0 {
				appendDeviceEventLogs(sl, drv.devices[i], flags, ts)
			}
		}
		if err := r.consumer.ConsumeLogs(ctx, ld); err != nil {
			r.logger.Warnw("ConsumeLogs failed", zap.Error(err))
		}

		// Rescan on DEVICE_ATTACH, after ConsumeLogs so that events are
		// delivered before re-init (case of hangs or crashes).
		// NOTE: rescan is inline here for simplicity; it stalls event processing
		// for this driver briefly. If that proves disruptive, move to a
		// dedicated per-device worker goroutine with a coalescing channel.
		for i, flags := range deviceEvents {
			if flags&l0sysman.EventTypeFlags(l0sysman.EVENT_TYPE_FLAG_DEVICE_ATTACH) != 0 {
				dev := drv.devices[i]
				dev.Lock()
				r.logger.Infow("Rescanning device on DEVICE_ATTACH", "deviceAttributes", dev.attributes)
				if err := dev.init(); err != nil {
					r.logger.Errorw("Device rescan failed", zap.Error(err))
				} else {
					r.logger.Debugw("Device rescanned successfully", "deviceAttributes", dev.attributes)
				}
				dev.Unlock()
			}
		}
	}
}

func appendDeviceEventLogs(sl plog.ScopeLogs, dev *device, flags l0sysman.EventTypeFlags, ts pcommon.Timestamp) {
	dev.RLock()
	defer dev.RUnlock()
	attrs := dev.attributes

	for _, flag := range flags.Bits() {
		lr := sl.LogRecords().AppendEmpty()
		lr.SetTimestamp(ts)
		sev := eventSeverity(flag)
		lr.SetSeverityNumber(sev)
		lr.SetSeverityText(sev.String())
		lr.SetEventName(common.EventNamePrefix + flag.String())
		lr.Body().SetStr(flag.String())

		lrAttrs := lr.Attributes()
		lrAttrs.PutStr("hw.id", attrs.hwID)
		lrAttrs.PutStr("hw.name", attrs.hwName)
		lrAttrs.PutStr("hw.model", attrs.hwModel)
		lrAttrs.PutStr("pci.bdf", attrs.pciBDF)
		lrAttrs.PutStr("pci.device_id", attrs.pciDeviceID)
		lrAttrs.PutStr("pci.vendor_id", attrs.pciVendorID)
	}
}

// eventSeverity maps a Sysman event type to an OTel log severity level.
func eventSeverity(flag l0sysman.EventTypeFlag) plog.SeverityNumber {
	switch flag {
	// TODO: review severity levels after this has been in use
	case l0sysman.EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS,
		l0sysman.EVENT_TYPE_FLAG_TEMP_CRITICAL,
		l0sysman.EVENT_TYPE_FLAG_DEVICE_DETACH,
		l0sysman.EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED,
		l0sysman.EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED:
		return plog.SeverityNumberError

	case l0sysman.EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS,
		l0sysman.EVENT_TYPE_FLAG_FREQ_THROTTLED,
		l0sysman.EVENT_TYPE_FLAG_TEMP_THRESHOLD1,
		l0sysman.EVENT_TYPE_FLAG_TEMP_THRESHOLD2,
		l0sysman.EVENT_TYPE_FLAG_MEM_HEALTH,
		l0sysman.EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH,
		l0sysman.EVENT_TYPE_FLAG_PCI_LINK_HEALTH,
		l0sysman.EVENT_TYPE_FLAG_ENERGY_THRESHOLD_CROSSED:
		return plog.SeverityNumberWarn

	default:
		return plog.SeverityNumberInfo
	}
}
