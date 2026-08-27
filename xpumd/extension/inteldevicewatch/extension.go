//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package inteldevicewatch

import (
	"context"
	"fmt"
	"sync"
	"time"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/extension"
	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/metric"
	"go.uber.org/zap"
	"go.uber.org/zap/zapcore"

	"github.com/intel/xpumanager/xpumd/extension/inteldevicewatch/internal/metadata"
	"github.com/intel/xpumanager/xpumd/extension/inteldevicewatch/internal/sysdev"
)

// attributeKeyState is the attribute the device state gauge uses.
const attributeKeyState = attribute.Key("state")

var allDevNodeStates = []sysdev.DevNodeState{
	sysdev.DevNodeStateOK,
	sysdev.DevNodeStateMissing,
	sysdev.DevNodeStateMismatch,
	sysdev.DevNodeStateStale,
	sysdev.DevNodeStateDenied,
	sysdev.DevNodeStateOrphan,
	sysdev.DevNodeStateBlocked,
	sysdev.DevNodeStateError,
}

// Error messages shared with the tests.
const (
	msgWatching         = "watching for device changes"
	msgDeviceSetChanged = "device set changed since sysman initialization"

	msgDeviceSetStale    = "the devices are no longer the ones enumerated at startup, restart xpumd to pick it up"
	msgDeviceSetRestored = "the devices match the ones enumerated at startup again"

	msgScanFailed    = "device inventory scan failed"
	msgScanRecovered = "device inventory scan succeeding again"

	msgDevicesDenied  = "device nodes exist but opening them is not permitted, recreate the pod to have the devices allocated to it"
	msgDevicesAllowed = "all device nodes are accessible again"
	msgNodesStale     = "device node directory is out of sync with sysfs, nodes refer to devices that are not there"
	msgNodesSynced    = "device node directory is in sync with sysfs again"
	msgNodesMissing   = "devices present in sysfs have no device node, restart the container to have the nodes created"
	msgNodesComplete  = "all devices in sysfs have device nodes"
	msgNodesBlocked   = "device node paths are taken by entries that are not device nodes, remove them by hand"
	msgNodesUnblocked = "device node paths are no longer taken by other entries"
	msgProbeFailed    = "device nodes could not be probed"
	msgProbeRecovered = "all device nodes could be probed again"
)

// deviceConditions is a mapping/config of device node states to the log reporting.
var deviceConditions = []struct {
	key      string
	states   []sysdev.DevNodeState
	level    zapcore.Level
	message  string
	resolved string
}{
	{
		key:      "devices_denied",
		states:   []sysdev.DevNodeState{sysdev.DevNodeStateDenied},
		level:    zapcore.WarnLevel,
		message:  msgDevicesDenied,
		resolved: msgDevicesAllowed,
	},
	{
		key:      "nodes_stale",
		states:   []sysdev.DevNodeState{sysdev.DevNodeStateMismatch, sysdev.DevNodeStateStale, sysdev.DevNodeStateOrphan},
		level:    zapcore.WarnLevel,
		message:  msgNodesStale,
		resolved: msgNodesSynced,
	},
	{
		key:      "nodes_missing",
		states:   []sysdev.DevNodeState{sysdev.DevNodeStateMissing},
		level:    zapcore.InfoLevel,
		message:  msgNodesMissing,
		resolved: msgNodesComplete,
	},
	{
		key:      "nodes_blocked",
		states:   []sysdev.DevNodeState{sysdev.DevNodeStateBlocked},
		level:    zapcore.WarnLevel,
		message:  msgNodesBlocked,
		resolved: msgNodesUnblocked,
	},
	{
		key:      "probe_errors",
		states:   []sysdev.DevNodeState{sysdev.DevNodeStateError},
		level:    zapcore.WarnLevel,
		message:  msgProbeFailed,
		resolved: msgProbeRecovered,
	},
}

type deviceSet struct {
	Fingerprint string
	Devices     sysdev.Inventory
}

// newDeviceSet describes the devices of one scan.
func newDeviceSet(inv sysdev.Inventory) *deviceSet {
	return &deviceSet{Fingerprint: inv.Fingerprint(), Devices: inv}
}

// pending describes the state of a change that has not yet settled.
type pending struct {
	Fingerprint string
	Count       int
}

// deviceWatch watches for changes in the set of devices this process started with.
type deviceWatch struct {
	cfg       *Config
	logger    *zap.Logger
	telemetry *metadata.TelemetryBuilder
	scanner   *sysdev.Scanner
	reporter  *reporter

	// baseline is the set the process started with
	baseline *deviceSet
	// pending changes
	pending pending
	// stale is the set a change settled on, and nil while the device set matches the baseline
	stale *deviceSet

	// overridable for testing
	newTicker func(time.Duration) (<-chan time.Time, func())

	cancel context.CancelFunc
	wg     sync.WaitGroup
}

var _ extension.Extension = (*deviceWatch)(nil)

func newDeviceWatch(settings extension.Settings, cfg *Config) (*deviceWatch, error) {
	scanner, err := cfg.scanner()
	if err != nil {
		return nil, err
	}
	telemetry, err := metadata.NewTelemetryBuilder(settings.TelemetrySettings)
	if err != nil {
		return nil, fmt.Errorf("creating telemetry builder: %w", err)
	}

	return &deviceWatch{
		cfg:       cfg,
		logger:    settings.Logger,
		telemetry: telemetry,
		scanner:   scanner,
		reporter:  newReporter(settings.Logger, cfg.ReportInterval),
		newTicker: realTicker,
	}, nil
}

func realTicker(interval time.Duration) (<-chan time.Time, func()) {
	ticker := time.NewTicker(interval)
	return ticker.C, ticker.Stop
}

// Start starts the watch loop.
func (w *deviceWatch) Start(ctx context.Context, _ component.Host) error {
	// Initial scan here, not on the first tick. It is the one the baseline
	// comes from, and extensions start before the pipeline, so this is as
	// close as it gets to the zesInit elsewhere.
	// NOTE: a failure is reported and retried, not to bail out (the extension is an aid,
	// not a prerequisite for collection).
	w.scan(ctx)

	watchCtx, cancel := context.WithCancel(context.Background())
	w.cancel = cancel
	w.wg.Go(func() { w.run(watchCtx) })
	return nil
}

// Shutdown stops the watch loop.
func (w *deviceWatch) Shutdown(context.Context) error {
	if w.cancel != nil {
		w.cancel()
	}
	w.wg.Wait()
	w.telemetry.Shutdown()
	return nil
}

func (w *deviceWatch) run(ctx context.Context) {
	tick, stop := w.newTicker(w.cfg.ScanInterval)
	defer stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-tick:
			w.scan(ctx)
		}
	}
}

// scan performs one comparison of sysfs against the device nodes and acts on the result.
func (w *deviceWatch) scan(ctx context.Context) {
	w.telemetry.DeviceWatchScans.Add(ctx, 1)

	inv, err := w.scanner.Scan()
	if err != nil {
		w.reportScanError(ctx, inv, err)
		return
	}
	w.recordCounts(ctx, inv)

	set := newDeviceSet(inv)
	w.settle(ctx, set)
	w.reporter.report(w.conditions(inv))
}

// reportScanError reports a scan that could not read the whole device tree.
// Scan drops a whole subsystem it could not list, so e.g. with one subsystem
// a failure looks exactly like every GPU having vanished.
func (w *deviceWatch) reportScanError(ctx context.Context, inv sysdev.Inventory, err error) {
	w.telemetry.DeviceWatchScanErrors.Add(ctx, 1)
	conds := []condition{{
		Key:      "scan_error",
		Level:    zapcore.WarnLevel,
		Message:  msgScanFailed,
		Digest:   err.Error(),
		Fields:   []zap.Field{zap.Error(err)},
		Resolved: msgScanRecovered,
	}}
	w.reporter.reportPartial(append(conds, w.conditions(inv)...))
}

// recordCounts updates the device_watch_devices metrics.
func (w *deviceWatch) recordCounts(ctx context.Context, inv sysdev.Inventory) {
	counts := inv.Counts()
	for _, state := range allDevNodeStates {
		w.telemetry.DeviceWatchDevices.Record(ctx, int64(counts[state]),
			metric.WithAttributes(attributeKeyState.String(string(state))))
	}
}

// settle updates the watch state based on the scan result.
func (w *deviceWatch) settle(ctx context.Context, set *deviceSet) {
	if w.baseline == nil {
		// Nothing to compare against yet, so this scan becomes the set the process
		// is running with: the closest thing to what zesInit enumerated. A change
		// before it is missed, which is the startup limitation the design documents.
		// TODO: an empty baseline means the process is monitoring nothing, and the
		// first device to appear costs a restart. Postponing zesInit until there is
		// a device to enumerate would save that restart.
		w.baseline = set
		w.logger.Info(msgWatching,
			zap.Duration("scan_interval", w.cfg.ScanInterval),
			zap.Strings("devices", set.Devices.IDs()),
			zap.String("fingerprint", set.Fingerprint))
		return
	}

	switch set.Fingerprint {
	case w.baseline.Fingerprint:
		// Back to what we started with. A device that flapped and returned needs no
		// restart: the receiver still holds a valid handle, and the DEVICE_ATTACH event
		// makes the receiver rescan the device and make it usable again.
		w.pending = pending{}
		w.stale = nil
		return
	case w.pending.Fingerprint:
		w.pending.Count++
	default:
		w.pending = pending{Fingerprint: set.Fingerprint, Count: 1}
	}

	if w.pending.Count < w.cfg.SettleScans {
		w.logger.Debug("device set change not settled yet",
			zap.String("fingerprint", set.Fingerprint),
			zap.Int("consecutive_scans", w.pending.Count),
			zap.Int("settle_scans", w.cfg.SettleScans))
		return
	} else if w.pending.Count == w.cfg.SettleScans {
		// Only the scan that reaches the threshold announces the change.
		// NOTE: The mismatch itself persists (and the reporter repeats it at the report interval).
		fields := append(diffFields(w.baseline, set),
			zap.String("fingerprint", set.Fingerprint),
			zap.String("baseline_fingerprint", w.baseline.Fingerprint))
		w.telemetry.DeviceWatchDeviceChanges.Add(ctx, 1)
		w.logger.Info(msgDeviceSetChanged, fields...)
	}

	// Settled on something other than the baseline
	w.stale = set
}

// conditions converts the device inventory into a set of conditions for reporting.
func (w *deviceWatch) conditions(inv sysdev.Inventory) []condition {
	var conds []condition

	for _, dc := range deviceConditions {
		devs := inv.ByState(dc.states...)
		if len(devs) == 0 {
			continue
		}
		conds = append(conds, condition{
			Key:     dc.key,
			Level:   dc.level,
			Message: dc.message,
			// Digested by what the devices are, not by how they are rendered: the
			// wording of a log line is nobody's change-detection key.
			Digest:   devs.Fingerprint(),
			Fields:   []zap.Field{zap.Strings("devices", devs.IDs())},
			Resolved: dc.resolved,
		})
	}

	if w.stale != nil {
		conds = append(conds, condition{
			Key:      "device_set_stale",
			Level:    zapcore.WarnLevel,
			Message:  msgDeviceSetStale,
			Digest:   w.stale.Fingerprint,
			Fields:   diffFields(w.baseline, w.stale),
			Resolved: msgDeviceSetRestored,
		})
	}

	return conds
}

// diffFields describes how one device set differs from another, for structured logging.
func diffFields(from, to *deviceSet) []zap.Field {
	diff := from.Devices.Diff(to.Devices)
	return []zap.Field{
		zap.Strings("appeared", diff.Added),
		zap.Strings("disappeared", diff.Removed),
		zap.Strings("changed", diff.Changed),
	}
}
