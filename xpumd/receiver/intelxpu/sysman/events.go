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
	l0intel "github.com/intel/level-zero-go/sysman/exp/intel"
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

func newSysmanEventsReceiver(devices *deviceRegistry, cfg *Config, logger *zap.SugaredLogger, nextConsumer consumer.Logs) (*sysmanEventsReceiver, error) {
	r := &sysmanEventsReceiver{}

	for i, drv := range devices.drivers {
		r.listeners = append(r.listeners, newDriverEventListener(i, drv, cfg, logger, nextConsumer))
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

const (
	// listenTimeout is the maximum time to wait for event(s) in EventListenEx.
	listenTimeout = 1 * time.Second

	// rescanSettlePeriod is the time required for no further DEVICE_ATTACH
	// events before a device rescan is triggered.
	rescanSettlePeriod = 2 * time.Second
	// deviceAttachEventFloodDelay is the period after which a non-settled streak of
	// DEVICE_ATTACH events for a single device is reported as a driver issue.
	deviceAttachEventFloodDelay = 10 * time.Second
)

type deviceRescanRequest struct {
	// events is the number of the accumulated DEVICE_ATTACH events
	events int
	// firstEventAt is the time of the first DEVICE_ATTACH event arrived
	firstEventAt time.Time
	// settledAt is the time the events are considered settled at, extended by every new DEVICE_ATTACH event
	settledAt time.Time
	// floodAt is the time a burst of events is reported as a flood
	floodAt time.Time
}

// addEvent adds one DEVICE_ATTACH event to the request, restarting the settle period.
func (p *deviceRescanRequest) addEvent(now time.Time) {
	if p.events == 0 {
		p.firstEventAt = now
		p.floodAt = now.Add(deviceAttachEventFloodDelay)
	}
	p.events++
	p.settledAt = now.Add(rescanSettlePeriod)
}

// isSettled tells whether a rescan is pending and its settle period has passed.
func (p *deviceRescanRequest) isSettled(now time.Time) bool {
	return p.events > 0 && !now.Before(p.settledAt)
}

// isFlood tells whether a stream of events is considered a flood.
func (p *deviceRescanRequest) isFlood(now time.Time) bool {
	return p.events != 0 && !now.Before(p.floodAt)
}

// resetFloodDelay re-arms the flood delay (so that flood is not reported before the delay period has passed again).
func (p *deviceRescanRequest) resetFloodDelay(now time.Time) {
	p.floodAt = now.Add(deviceAttachEventFloodDelay)
}

// driverEventListener listens for the events of one Sysman driver and emits them,
// together with the info log records (if available).
type driverEventListener struct {
	// drvIdx identifies the driver in the logged messages.
	drvIdx int
	drv    *driver
	// l0Devices are the devices of the driver, in the form the listen calls take.
	l0Devices []*l0sysman.Device
	// infoLogsEnabled tells whether the info log records of the driver are
	// collected, read and listened for.
	infoLogsEnabled bool
	// pendingRescans contains rescan requests for devices, indexed like drv.devices
	pendingRescans []deviceRescanRequest
	consumer       consumer.Logs
	logger         *zap.SugaredLogger
}

func newDriverEventListener(index int, drv *driver, cfg *Config, logger *zap.SugaredLogger, nextConsumer consumer.Logs) *driverEventListener {
	l0Devices := make([]*l0sysman.Device, len(drv.devices))
	for i, dev := range drv.devices {
		l0Devices[i] = dev.Device
	}

	return &driverEventListener{
		drvIdx:          index,
		drv:             drv,
		l0Devices:       l0Devices,
		infoLogsEnabled: cfg.InfoLogs.Enabled && len(drv.infoLogs) > 0,
		pendingRescans:  make([]deviceRescanRequest, len(drv.devices)),
		consumer:        nextConsumer,
		logger:          logger,
	}
}

// run registers for the events of the driver and listens for them until the
// context is cancelled.
// NOTE: info logs are handled (enabled, listened, read) in the same goroutine to
// guarantee serialized access.
func (l *driverEventListener) run(ctx context.Context) {
	l.registerDeviceEvents()

	if l.infoLogsEnabled {
		// Can't be collected if the event triggering the reads is not available
		l.infoLogsEnabled = l.enableInfoLogs()
		defer l.disableInfoLogs()
	}

	var prevIdleTime time.Time
	for {
		select {
		case <-ctx.Done():
			return
		default:
		}

		hasDeviceEvents, deviceEvents, driverEvents, err := l.listen()
		// TODO: revisit error handling, bail out on all errors except perhaps
		// RESULT_ERROR_DEVICE_LOST, rescan drivers and devices on some others(?)
		if err != nil {
			l.logger.Warnw("Event listening failed", zap.Error(err))
			// Brief back-off to avoid flooding on persistent errors.
			select {
			case <-time.After(5 * time.Second):
			case <-ctx.Done():
				return
			}
			continue
		}
		if hasDeviceEvents {
			l.emitDeviceEvents(ctx, deviceEvents)
			l.requestRescans(deviceEvents)
		}

		// Rescan the devices whose events have settled.
		l.rescanPendingDevices()

		infoLogRecords := 0
		// Check for infoLogsEnabled too, just in case a sporadic driver event is received
		if driverEvents&l0sysman.EventTypeFlags(l0intel.CPER_DATA_AVAILABLE) != 0 && l.infoLogsEnabled {
			infoLogRecords = l.readInfoLogs(ctx)
		}

		if !hasDeviceEvents && infoLogRecords == 0 {
			if time.Since(prevIdleTime) < listenTimeout/2 {
				time.Sleep(listenTimeout)
			}
			prevIdleTime = time.Now()
		}
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
				zap.Error(err), "deviceID", i+1, "attributes", dev.attributes)
			continue
		}
		l.logger.Debugw("Registered Sysman device events", "eventsMask", eventsMask, "attributes", dev.attributes)
		registered++
	}
	l.logger.Infow("Sysman devices registered for events", "registered", registered,
		"enumerated", len(l.drv.devices), "driverIndex", l.drvIdx)
}

func (l *driverEventListener) listen() (hasDeviceEvents bool, deviceEvents []l0sysman.EventTypeFlags,
	driverEvents l0sysman.EventTypeFlags, err error) {
	if l.infoLogsEnabled {
		return l.drv.EventListenExp(listenTimeout, l.l0Devices)
	}
	deviceCount, deviceEvents, err := l.drv.EventListenEx(listenTimeout, l.l0Devices)
	return deviceCount != 0, deviceEvents, 0, err
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

// requestRescans requests a rescan of the devices that reported DEVICE_ATTACH.
func (l *driverEventListener) requestRescans(deviceEvents []l0sysman.EventTypeFlags) {
	now := time.Now()
	for i, flags := range deviceEvents {
		if flags&l0sysman.EventTypeFlags(l0sysman.EVENT_TYPE_FLAG_DEVICE_ATTACH) != 0 {
			l.pendingRescans[i].addEvent(now)
		}
	}
}

// rescanPendingDevices re-initializes the devices with a pending rescan request whose events have settled.
func (l *driverEventListener) rescanPendingDevices() {
	// NOTE: rescan is inline here for simplicity; it stalls event processing for this
	// driver briefly. If that proves disruptive, move to a dedicated per-device worker
	// goroutine fed by the requests.
	now := time.Now()
	for i := range l.pendingRescans {
		p := &l.pendingRescans[i]
		if !p.isSettled(now) {
			if p.isFlood(now) {
				p.resetFloodDelay(now)
				l.logger.Warnw("Continuous DEVICE_ATTACH events: device rescan postponed until they settle",
					"attachEvents", p.events, "duration", now.Sub(p.firstEventAt), "deviceID", i+1)
			}
			continue
		}
		events := p.events
		*p = deviceRescanRequest{}

		dev := l.drv.devices[i]
		dev.Lock()
		l.logger.Infow("Rescanning device on DEVICE_ATTACH", "attachEvents", events,
			"deviceID", i+1, "attributes", dev.attributes)
		if err := dev.init(); err != nil {
			l.logger.Errorw("Device rescan failed", zap.Error(err))
		} else {
			l.logger.Debugw("Device rescanned successfully", "attributes", dev.attributes)
		}
		dev.Unlock()
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
		lrAttrs.PutStr("hw.id", attrs.HwID)
		lrAttrs.PutStr("hw.name", attrs.HwName)
		lrAttrs.PutStr("hw.model", attrs.HwModel)
		lrAttrs.PutStr("pci.bdf", attrs.PciBDF)
		lrAttrs.PutStr("pci.device_id", attrs.PciDeviceID)
		lrAttrs.PutStr("pci.vendor_id", attrs.PciVendorID)
	}
}

// infoLogEventName is the event name of the emitted info log records
const infoLogEventName = common.EventNamePrefix + "info_log.cper"

// enableInfoLogs registers the CPER data available event of the driver, i.e. the
// notification that triggers the reads, and enables the collection of the records
// of its info logs. Returns false when nothing can be collected, i.e. when the
// event is not available or when no info log could be enabled.
func (l *driverEventListener) enableInfoLogs() bool {
	if err := l.drv.EventRegister(l0sysman.EventTypeFlags(l0intel.CPER_DATA_AVAILABLE)); err != nil {
		l.logger.Errorw("Driver EventRegister() failed: info log records not collected", zap.Error(err))
		return false
	}

	for _, src := range l.drv.infoLogs {
		src.enable()
	}
	if l.enabledInfoLogs() == 0 {
		l.logger.Errorw("No info log collection could be enabled: info log records not collected",
			"infoLogs", len(l.drv.infoLogs), "driverIndex", l.drvIdx)
		return false
	}
	return true
}

// enabledInfoLogs returns the number of info logs of the driver whose record
// collection is enabled.
func (l *driverEventListener) enabledInfoLogs() int {
	enabled := 0
	for _, src := range l.drv.infoLogs {
		if src.enabled {
			enabled++
		}
	}
	return enabled
}

// disableInfoLogs disables the collection of the records of every info log of the
// driver.
func (l *driverEventListener) disableInfoLogs() {
	for _, src := range l.drv.infoLogs {
		src.disable()
	}
}

// readInfoLogs reads the pending info log records of the driver and emits them as
// logs. Returns the number of records read.
//
// An info log that fails to read is dropped (see infoLogSource.read). Once the
// last one is gone, the collection of the driver ends: there is nothing left to
// read, so the event triggering the reads is not listened for anymore either.
func (l *driverEventListener) readInfoLogs(ctx context.Context) int {
	read := 0
	for _, src := range l.drv.infoLogs {
		records := src.read()
		if len(records) > 0 {
			l.emitInfoLogs(ctx, records)
			read += len(records)
		}
	}

	if l.enabledInfoLogs() == 0 {
		l.logger.Errorw("No info log of the driver is collected anymore: info log records no longer read",
			"infoLogs", len(l.drv.infoLogs), "driverIndex", l.drvIdx)
		l.infoLogsEnabled = false
	}
	return read
}

// emitInfoLogs emits the given info log records of the driver as logs.
func (l *driverEventListener) emitInfoLogs(ctx context.Context, records []l0intel.InfoLogRecord) {
	ld := plog.NewLogs()
	rl := ld.ResourceLogs().AppendEmpty()
	sl := rl.ScopeLogs().AppendEmpty()
	sl.Scope().SetName(metadata.ScopeName)

	observed := pcommon.NewTimestampFromTime(time.Now())
	devices := deviceAttributesByBDF(l.drv)

	for _, rec := range records {
		bdf := pciBDF(rec.Metadata.Address)
		appendInfoLogRecord(sl, rec, bdf, devices[bdf], observed)
	}

	if err := l.consumer.ConsumeLogs(ctx, ld); err != nil {
		l.logger.Warnw("ConsumeLogs failed", zap.Error(err))
	}
}

func deviceAttributesByBDF(drv *driver) map[string]*deviceAttributes {
	devices := make(map[string]*deviceAttributes, len(drv.devices))

	for _, dev := range drv.devices {
		dev.RLock()
		attrs := dev.attributes
		dev.RUnlock()
		devices[attrs.PciBDF] = &attrs
	}

	return devices
}

// appendInfoLogRecord appends one info log record to scope logs.
func appendInfoLogRecord(sl plog.ScopeLogs, rec l0intel.InfoLogRecord, bdf string,
	dev *deviceAttributes, observed pcommon.Timestamp) {
	lr := sl.LogRecords().AppendEmpty()

	lr.SetObservedTimestamp(observed)
	// NOTE: the timestamp of the record is microseconds since boot, so we
	// cannot reliably convert it to wall clock time. This timestamp is added as
	// an attribute and the observation time is used as the timestamp of the
	// record.
	lr.SetTimestamp(observed)
	// NOTE: static severity. The records report hardware errors, but their
	// severity is part of the record itself, not of the metadata we get.
	lr.SetSeverityNumber(plog.SeverityNumberError)
	lr.SetSeverityText(plog.SeverityNumberError.String())
	lr.SetEventName(infoLogEventName)
	// The record is passed on as-is
	lr.Body().SetEmptyBytes().FromRaw(rec.Data)

	attrs := lr.Attributes()
	attrs.PutInt("cper.timestamp_us", int64(rec.Metadata.Timestamp))
	attrs.PutStr("cper.platform_id", rec.Metadata.Uuid.Id.String())
	attrs.PutStr("pci.bdf", bdf)

	if dev != nil {
		attrs.PutStr("hw.id", dev.HwID)
		attrs.PutStr("hw.name", dev.HwName)
		attrs.PutStr("hw.model", dev.HwModel)
		attrs.PutStr("pci.device_id", dev.PciDeviceID)
		attrs.PutStr("pci.vendor_id", dev.PciVendorID)
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
