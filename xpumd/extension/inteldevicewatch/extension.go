//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package inteldevicewatch

import (
	"context"
	"fmt"
	"slices"
	"strings"
	"sync"
	"time"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/component/componentstatus"
	"go.opentelemetry.io/collector/extension"
	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/metric"
	"go.uber.org/zap"
	"go.uber.org/zap/zapcore"

	"github.com/intel/xpumanager/xpumd/extension/inteldevicewatch/internal/metadata"
	"github.com/intel/xpumanager/xpumd/extension/inteldevicewatch/internal/sysdev"
)

const (
	// attributeKeyState is the attribute the device state gauge uses.
	attributeKeyState = attribute.Key("state")
	// attributeKeyReason is the attribute the suppressed exit counter uses.
	attributeKeyReason = attribute.Key("reason")
)

// Why an exit a settled change called for was not requested.
const (
	reasonMaxRestarts = "max_restarts"
	reasonStateError  = "state_error"
)

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
	msgShuttingDown     = "shutting down the collector"

	msgChangeNotRestartable = "device set changed but a restart would find the same devices, only reporting it"
	msgExitIneffective      = "restarting did not change the device nodes of this container, re-create the pod to have them re-created"
	msgStateSaveFailed      = "the restart record could not be persisted, not exiting: an exit that goes uncounted cannot be rate limited"

	msgDeviceSetStale        = "the devices are no longer the ones enumerated at startup"
	msgDeviceSetStaleRestart = msgDeviceSetStale + ", restart xpumanager to re-enumerate"
	msgDeviceSetRestored     = "the devices match the ones enumerated at startup again"

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

// errDeviceSetChanged is the fatal error handed to the collector when it exits
// over a device change, and so ends up in the collector's own exit message.
const errDeviceSetChanged = "device set changed since sysman initialization, exiting to re-enumerate (appeared: %q, disappeared: %q, changed: %q)"

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
	limiter   *restartLimiter

	// host is used for shutting down the collector
	host component.Host
	// exitDisabled is set once the restart rate limit has been hit
	exitDisabled bool

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
		limiter:   newRestartLimiter(settings.Logger, cfg.StateFile, cfg.MaxRestarts, cfg.RestartWindow),
		newTicker: realTicker,
	}, nil
}

func realTicker(interval time.Duration) (<-chan time.Time, func()) {
	ticker := time.NewTicker(interval)
	return ticker.C, ticker.Stop
}

// Start starts the watch loop.
func (w *deviceWatch) Start(ctx context.Context, host component.Host) error {
	w.host = host

	if w.cfg.ChangeAction == ActionExit {
		w.limiter.init()
	}

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
			zap.String("action", string(w.cfg.ChangeAction)),
			zap.Duration("scan_interval", w.cfg.ScanInterval),
			zap.Strings("devices", set.Devices.IDs()),
			zap.String("fingerprint", set.Fingerprint))

		// Restart didn't change the set of "fixable" devices, don't try again.
		if unfixed := set.Devices.Fixable(); len(unfixed) > 0 && set.Fingerprint == w.limiter.state.ExitFingerprint {
			w.logger.Warn(msgExitIneffective,
				zap.Strings("devices", unfixed.IDs()),
				zap.String("fingerprint", set.Fingerprint))
		}
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
		// Only the scan that reaches the threshold acts on the change.
		// NOTE: The mismatch itself persists (and the reporter repeats it at the report interval).
		w.actOnChange(ctx, set, w.baseline.Devices.Diff(set.Devices))
	}

	// Settled on something other than the baseline
	w.stale = set
}

// actOnChange reports a settled difference from the baseline and, in exit mode
// requests the collector to shut down if the change is something a restart could fix.
func (w *deviceWatch) actOnChange(ctx context.Context, set *deviceSet, diff sysdev.InventoryDiff) {
	fields := append(diffFields(diff),
		zap.String("fingerprint", set.Fingerprint),
		zap.String("baseline_fingerprint", w.baseline.Fingerprint))

	w.telemetry.DeviceWatchDeviceChanges.Add(ctx, 1)
	w.logger.Info(msgDeviceSetChanged, fields...)

	// NOTE: exitDisabled is deliberately a one-way switch. The logic
	// behind is that the limiter window reopening is no evidence that
	// whatever caused the limit to be reached has been fixed.
	if w.cfg.ChangeAction != ActionExit || w.exitDisabled {
		return
	}
	if !restartable(diff, set.Devices) {
		w.logger.Warn(msgChangeNotRestartable, fields...)
		return
	}
	w.requestExit(ctx, set, diff)
}

// restartable reports whether a settled diff is something that a restart could fix, at least partly.
func restartable(diff sysdev.InventoryDiff, current sysdev.Inventory) bool {
	if len(diff.Removed) > 0 {
		// A device the process enumerated is gone. Only re-enumerating drops it,
		// whatever state the devices that are left are in.
		return true
	}

	devices := current.ByID()
	for _, id := range slices.Concat(diff.Added, diff.Changed) {
		switch state := devices[id].State; {
		case state == sysdev.DevNodeStateOK, state.Fixable():
			return true
		}
	}
	return false
}

// requestExit asks the collector to shut down.
func (w *deviceWatch) requestExit(ctx context.Context, set *deviceSet, diff sysdev.InventoryDiff) {
	allowed, count, err := w.limiter.record(set.Fingerprint)
	if err != nil {
		// Refuse to exit if the state file cannot be updated. The restart
		// limiter is a safety valve, and if it cannot be updated the new
		// process cannot know how many restarts have happened (and we'd likely be in a restart loop).
		w.telemetry.DeviceWatchExitsSuppressed.Add(ctx, 1, metric.WithAttributes(attributeKeyReason.String(reasonStateError)))
		w.logger.Error(msgStateSaveFailed, zap.Error(err))
		// Let the change settle again, so that a later scan retries the exit.
		w.pending = pending{}
		return
	}
	if !allowed {
		// NOTE: the limiter logs why it refused.
		w.exitDisabled = true
		w.telemetry.DeviceWatchExitsSuppressed.Add(ctx, 1, metric.WithAttributes(attributeKeyReason.String(reasonMaxRestarts)))
		return
	}

	fields := diffFields(diff)
	if w.cfg.StateFile == "" {
		fields = append(fields, zap.String("rate_limit", "disabled"))
	} else {
		fields = append(fields,
			zap.Int("restarts", count),
			zap.Int("max_restarts", w.cfg.MaxRestarts))
	}

	w.telemetry.DeviceWatchExitsRequested.Add(ctx, 1)
	w.logger.Warn(msgShuttingDown, fields...)

	// A fatal error event logs it and shuts the collector down gracefully.
	// NOTE: not os.Exit: we want the other components to shut down cleanly.
	componentstatus.ReportStatus(w.host, componentstatus.NewFatalErrorEvent(
		fmt.Errorf(errDeviceSetChanged,
			strings.Join(diff.Added, ", "), strings.Join(diff.Removed, ", "), strings.Join(diff.Changed, ", "))))
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
		message := msgDeviceSetStaleRestart
		if w.cfg.ChangeAction == ActionExit {
			message = msgDeviceSetStale
		}
		conds = append(conds, condition{
			Key:      "device_set_stale",
			Level:    zapcore.WarnLevel,
			Message:  message,
			Digest:   w.stale.Fingerprint,
			Fields:   diffFields(w.baseline.Devices.Diff(w.stale.Devices)),
			Resolved: msgDeviceSetRestored,
		})
	}

	return conds
}

// diffFields describes a device set difference for structured logging.
func diffFields(diff sysdev.InventoryDiff) []zap.Field {
	return []zap.Field{
		zap.Strings("appeared", diff.Added),
		zap.Strings("disappeared", diff.Removed),
		zap.Strings("changed", diff.Changed),
	}
}
