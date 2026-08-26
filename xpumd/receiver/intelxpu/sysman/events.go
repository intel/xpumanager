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
	"github.com/intel/xpumanager/xpumd/receiver/intelxpu/sysman/internal/metadata"
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

// sysmanEventsReceiver runs one event listener per Sysman driver.
type sysmanEventsReceiver struct {
	listeners []*driverEventListener
	wg        sync.WaitGroup
	stop      context.CancelFunc
}

func newSysmanEventsReceiver(devices *deviceRegistry, logger *zap.SugaredLogger, nextConsumer consumer.Logs) (*sysmanEventsReceiver, error) {
	r := &sysmanEventsReceiver{}

	for i, drv := range devices.drivers {
		r.listeners = append(r.listeners, newDriverEventListener(i, drv, logger, nextConsumer))
	}

	return r, nil
}

// Start implements collector component.Component.Start.
func (r *sysmanEventsReceiver) Start(ctx context.Context, _ component.Host) error {
	ctx, r.stop = context.WithCancel(ctx)
	for _, l := range r.listeners {
		r.wg.Go(func() {
			l.run(ctx)
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

// driverEventListener listens for the events of one Sysman driver and emits them.
type driverEventListener struct {
	// drvIdx identifies the driver in the logged messages.
	drvIdx int
	drv    *driver
	// l0Devices are the devices of the driver, in the form the listen calls take.
	l0Devices []*l0sysman.Device
	consumer  consumer.Logs
	logger    *zap.SugaredLogger
}

func newDriverEventListener(index int, drv *driver, logger *zap.SugaredLogger, nextConsumer consumer.Logs) *driverEventListener {
	l0Devices := make([]*l0sysman.Device, len(drv.devices))
	for i, dev := range drv.devices {
		l0Devices[i] = dev.Device
	}

	return &driverEventListener{
		drvIdx:    index,
		drv:       drv,
		l0Devices: l0Devices,
		consumer:  nextConsumer,
		logger:    logger,
	}
}

// run registers for the events of the driver and listens for them until the
// context is cancelled.
func (l *driverEventListener) run(ctx context.Context) {
	l.registerDeviceEvents()

	var prevZeroTime time.Time
	for {
		select {
		case <-ctx.Done():
			return
		default:
		}

		deviceCount, deviceEvents, err := l.drv.driver.EventListenEx(listenTimeout, l.l0Devices)
		// TODO: revisit error handling, bail out on all errors except perhaps
		// RESULT_ERROR_DEVICE_LOST, rescan drivers and devices on some others(?)
		if err != nil {
			l.logger.Warnw("EventListenEx failed", zap.Error(err))
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

		l.emitDeviceEvents(ctx, deviceEvents)
		// Rescan on DEVICE_ATTACH, after ConsumeLogs so that events are
		// delivered before re-init (case of hangs or crashes).
		l.rescanAttachedDevices(deviceEvents)
	}
}

// registerDeviceEvents registers all the known event types on every device of the
// driver.
func (l *driverEventListener) registerDeviceEvents() {
	registered := 0
	for i, dev := range l.drv.devices {
		eventsMask, err := dev.EventRegister(allEventTypeFlags)
		if err != nil {
			l.logger.Errorw("Device EventRegister() failed: device events unavailable",
				zap.Error(err), "deviceID", i+1, "deviceAttributes", dev.attributes)
			continue
		}
		l.logger.Debugw("Registered Sysman device events", "eventsMask", eventsMask, "deviceAttributes", dev.attributes)
		registered++
	}
	l.logger.Infow("Sysman devices registered for events", "registered", registered,
		"enumerated", len(l.drv.devices), "driverIndex", l.drvIdx)
}

// emitDeviceEvents emits the given device events of the driver as logs.
func (l *driverEventListener) emitDeviceEvents(ctx context.Context, deviceEvents []l0sysman.EventTypeFlags) {
	ld := plog.NewLogs()
	rl := ld.ResourceLogs().AppendEmpty()
	sl := rl.ScopeLogs().AppendEmpty()
	sl.Scope().SetName(metadata.ScopeName)

	ts := pcommon.NewTimestampFromTime(time.Now())

	for i, flags := range deviceEvents {
		if flags != 0 {
			appendDeviceEventLogs(sl, l.drv.devices[i], flags, ts)
		}
	}
	if err := l.consumer.ConsumeLogs(ctx, ld); err != nil {
		l.logger.Warnw("ConsumeLogs failed", zap.Error(err))
	}
}

// rescanAttachedDevices re-initializes the devices that reported DEVICE_ATTACH.
func (l *driverEventListener) rescanAttachedDevices(deviceEvents []l0sysman.EventTypeFlags) {
	// NOTE: rescan is synchronous for simplicity; it stalls event processing for this
	// driver briefly. If that proves disruptive, move to a dedicated per-device worker
	// goroutine with a coalescing channel.
	for i, flags := range deviceEvents {
		if flags&l0sysman.EventTypeFlags(l0sysman.EVENT_TYPE_FLAG_DEVICE_ATTACH) != 0 {
			dev := l.drv.devices[i]
			dev.Lock()
			l.logger.Infow("Rescanning device on DEVICE_ATTACH", "deviceAttributes", dev.attributes)
			if err := dev.init(); err != nil {
				l.logger.Errorw("Device rescan failed", zap.Error(err))
			} else {
				l.logger.Debugw("Device rescanned successfully", "deviceAttributes", dev.attributes)
			}
			dev.Unlock()
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
