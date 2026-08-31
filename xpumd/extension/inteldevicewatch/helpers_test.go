//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package inteldevicewatch

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"sync"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/component/componentstatus"
	"go.opentelemetry.io/collector/component/componenttest"
	"go.opentelemetry.io/collector/extension/extensiontest"
	"go.opentelemetry.io/otel/sdk/metric/metricdata"
	"go.uber.org/zap"
	"go.uber.org/zap/zapcore"
	"go.uber.org/zap/zaptest/observer"

	"github.com/intel/xpumanager/xpumd/extension/inteldevicewatch/internal/metadata"
	"github.com/intel/xpumanager/xpumd/extension/inteldevicewatch/internal/sysdev"
)

// drmMajor is the character device major number of DRM devices.
const drmMajor = 226

// The internal telemetry the extension records, named as the collector exports
// it. mdatagen does not generate constants for these.
const (
	metricDevices         = "otelcol_device_watch_devices"
	metricDeviceChanges   = "otelcol_device_watch_device_changes"
	metricScans           = "otelcol_device_watch_scans"
	metricScanErrors      = "otelcol_device_watch_scan_errors"
	metricExitsRequested  = "otelcol_device_watch_exits_requested"
	metricExitsSuppressed = "otelcol_device_watch_exits_suppressed"
)

// devTree is a fake sysfs plus device directory the test mutates between scans.
// Real device nodes need CAP_MKNOD, they are mocked through Scanner.StatNode.
type devTree struct {
	t     *testing.T
	sysfs string
	dev   string

	mu    sync.Mutex
	nodes map[string]sysdev.DevNum // keyed by device node name
	probe map[string]error         // keyed by device node name
}

func newDevTree(t *testing.T) *devTree {
	t.Helper()
	root := t.TempDir()
	tree := &devTree{
		t:     t,
		sysfs: filepath.Join(root, "sys"),
		dev:   filepath.Join(root, "dev"),
		nodes: map[string]sysdev.DevNum{},
		probe: map[string]error{},
	}
	require.NoError(t, os.MkdirAll(filepath.Join(tree.sysfs, "class", "drm"), 0o755))
	require.NoError(t, os.MkdirAll(filepath.Join(tree.dev, "dri"), 0o755))
	return tree
}

// addCard adds a GPU the way it "normally" is, i.e. in sysfs, with a matching device node.
func (d *devTree) addCard(name string, minor uint32) {
	d.t.Helper()
	d.addSysfsCard(name, minor)
	d.setDevNode(name, minor, true)
}

// addSysfsCard adds a GPU to sysfs only, leaving it without a device node.
func (d *devTree) addSysfsCard(name string, minor uint32) {
	d.t.Helper()
	pci := d.pciDir(minor)
	drmDir := filepath.Join(pci, "drm", name)
	require.NoError(d.t, os.MkdirAll(drmDir, 0o755))
	require.NoError(d.t, os.WriteFile(filepath.Join(pci, "vendor"), []byte("0x8086\n"), 0o644))
	require.NoError(d.t, os.WriteFile(filepath.Join(drmDir, "dev"),
		[]byte(sysdev.DevNum{Major: drmMajor, Minor: minor}.String()+"\n"), 0o644))
	require.NoError(d.t, os.Symlink(drmDir, filepath.Join(d.sysfs, "class", "drm", name)))
}

// removeSysfsCardSymlink takes a GPU out of the sysfs class directory (like the
// kernel does e.g. on unbind). Nothing else is touched, the device directory
// under sysfs/devices stays, and so does any device node.
func (d *devTree) removeSysfsCardSymlink(name string) {
	d.t.Helper()
	link := filepath.Join(d.sysfs, "class", "drm", name)
	info, err := os.Lstat(link)
	require.NoError(d.t, err)
	require.Equal(d.t, os.ModeSymlink, info.Mode()&os.ModeSymlink, "%s is not a symlink", link)
	require.NoError(d.t, os.Remove(link))
}

// pciDir is the sysfs directory of the PCI device a card hangs off.
func (d *devTree) pciDir(minor uint32) string {
	return filepath.Join(d.sysfs, "devices", "pci0000:00", fmt.Sprintf("0000:00:%02x.0", minor))
}

// setSysfsDriver binds a card's PCI device to a driver, the way sysfs shows a bind: a
// symlink to the driver's directory, replacing whatever it was bound to before.
func (d *devTree) setSysfsDriver(minor uint32, driver string) {
	d.t.Helper()
	link := filepath.Join(d.pciDir(minor), "driver")
	require.NoError(d.t, os.RemoveAll(link))
	require.NoError(d.t, os.Symlink(filepath.Join(d.sysfs, "bus", "pci", "drivers", driver), link))
}

func (d *devTree) setDevNode(name string, minor uint32, present bool) {
	d.t.Helper()
	d.mu.Lock()
	defer d.mu.Unlock()
	if present {
		require.NoError(d.t, os.WriteFile(filepath.Join(d.dev, "dri", name), nil, 0o644))
		d.nodes[name] = sysdev.DevNum{Major: drmMajor, Minor: minor}
		return
	}
	require.NoError(d.t, os.Remove(filepath.Join(d.dev, "dri", name)))
	delete(d.nodes, name)
}

func (d *devTree) setProbeResult(name string, err error) {
	d.mu.Lock()
	defer d.mu.Unlock()
	d.probe[name] = err
}

// addDevPlainFile puts an entry that is not a device node where a card's node belongs.
func (d *devTree) addDevPlainFile(name string) {
	d.t.Helper()
	d.mu.Lock()
	defer d.mu.Unlock()
	require.NoError(d.t, os.WriteFile(filepath.Join(d.dev, "dri", name), nil, 0o644))
	delete(d.nodes, name)
}

func (d *devTree) statNode(path string) (sysdev.NodeIdentity, error) {
	d.mu.Lock()
	defer d.mu.Unlock()
	if num, ok := d.nodes[filepath.Base(path)]; ok {
		return sysdev.NodeIdentity{Present: true, CharDev: true, DevNum: num}, nil
	}
	if _, err := os.Lstat(path); err != nil {
		return sysdev.NodeIdentity{}, err
	}
	return sysdev.NodeIdentity{Present: true}, nil
}

func (d *devTree) probeNode(path string) error {
	d.mu.Lock()
	defer d.mu.Unlock()
	return d.probe[filepath.Base(path)]
}

// watchFixture is a device watch wired to a fake device tree, an observed
// logger and an in-memory telemetry reader.
type watchFixture struct {
	watch *deviceWatch
	tree  *devTree
	logs  *observer.ObservedLogs
	tel   *componenttest.Telemetry
	host  *statusHost
}

func newWatchFixture(t *testing.T, mutate func(*Config)) *watchFixture {
	t.Helper()
	tree := newDevTree(t)

	cfg, ok := createDefaultConfig().(*Config)
	require.True(t, ok)
	cfg.SysfsRoot = tree.sysfs
	cfg.DevRoot = tree.dev
	cfg.StateFile = filepath.Join(t.TempDir(), "restarts.json")
	if mutate != nil {
		mutate(cfg)
	}
	require.NoError(t, cfg.Validate())

	core, logs := observer.New(zapcore.DebugLevel)
	tel := componenttest.NewTelemetry()
	t.Cleanup(func() { require.NoError(t, tel.Shutdown(context.Background())) })

	settings := extensiontest.NewNopSettings(metadata.Type)
	settings.TelemetrySettings = tel.NewTelemetrySettings()
	settings.Logger = zap.New(core)

	watch, err := newDeviceWatch(settings, cfg)
	require.NoError(t, err)
	watch.scanner.StatNode = tree.statNode
	watch.scanner.Probe = tree.probeNode

	// Ticker that never fires
	watch.newTicker = func(time.Duration) (<-chan time.Time, func()) {
		return make(chan time.Time), func() {}
	}

	return &watchFixture{
		watch: watch,
		tree:  tree,
		logs:  logs,
		tel:   tel,
		host:  &statusHost{},
	}
}

// restart builds a second watch on the same device tree and state file, standing
// in for the container being restarted.
func (f *watchFixture) restart(t *testing.T) *watchFixture {
	t.Helper()
	cfg := *f.watch.cfg
	next := newWatchFixture(t, func(c *Config) { *c = cfg })
	next.tree = f.tree
	next.watch.scanner.StatNode = f.tree.statNode
	next.watch.scanner.Probe = f.tree.probeNode
	return next
}

// start starts the watch and stops it when the test ends.
func (f *watchFixture) start(t *testing.T) {
	t.Helper()
	require.NoError(t, f.watch.Start(context.Background(), f.host))
	t.Cleanup(func() { require.NoError(t, f.watch.Shutdown(context.Background())) })
}

// scan runs one scan synchronously, bypassing the clock.
func (f *watchFixture) scan(n int) {
	for range n {
		f.watch.scan(context.Background())
	}
}

func (f *watchFixture) messages() []string {
	entries := f.logs.All()
	out := make([]string, 0, len(entries))
	for _, e := range entries {
		out = append(out, e.Message)
	}
	return out
}

// count returns how many times a message has been logged.
func (f *watchFixture) count(msg string) int {
	n := 0
	for _, m := range f.messages() {
		if m == msg {
			n++
		}
	}
	return n
}

// statusHost is a component.Host that records the status events reported to it,
// standing in for the collector service's async error channel.
type statusHost struct {
	mu     sync.Mutex
	events []*componentstatus.Event
}

var (
	_ component.Host           = (*statusHost)(nil)
	_ componentstatus.Reporter = (*statusHost)(nil)
)

func (h *statusHost) GetExtensions() map[component.ID]component.Component { return nil }

func (h *statusHost) Report(event *componentstatus.Event) {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.events = append(h.events, event)
}

func (h *statusHost) fatalErrors() []error {
	h.mu.Lock()
	defer h.mu.Unlock()
	var errs []error
	for _, e := range h.events {
		if e.Status() == componentstatus.StatusFatalError {
			errs = append(errs, e.Err())
		}
	}
	return errs
}

func gaugePoints(t *testing.T, m metricdata.Metrics) []metricdata.DataPoint[int64] {
	t.Helper()
	gauge, ok := m.Data.(metricdata.Gauge[int64])
	require.True(t, ok, "%s is not an int64 gauge but %T", m.Name, m.Data)
	return gauge.DataPoints
}

func assertDeviceGauge(t *testing.T, f *watchFixture, state sysdev.DevNodeState, want int64) {
	t.Helper()
	m, err := f.tel.GetMetric(metricDevices)
	require.NoError(t, err)
	found := false
	for _, dp := range gaugePoints(t, m) {
		if value, ok := dp.Attributes.Value(attributeKeyState); ok && value.AsString() == string(state) {
			assert.Equal(t, want, dp.Value, "state %s", state)
			found = true
		}
	}
	assert.True(t, found, "no data point for state %s", state)
}

func assertSuppressedExits(t *testing.T, f *watchFixture, reason string, want int64) {
	t.Helper()
	m, err := f.tel.GetMetric(metricExitsSuppressed)
	require.NoError(t, err)
	sum, ok := m.Data.(metricdata.Sum[int64])
	require.True(t, ok, "%s is not an int64 sum but %T", m.Name, m.Data)
	var total int64
	for _, dp := range sum.DataPoints {
		if value, ok := dp.Attributes.Value(attributeKeyReason); ok && value.AsString() == reason {
			total += dp.Value
		}
	}
	assert.Equal(t, want, total, "%s reason %s", metricExitsSuppressed, reason)
}

func assertCounter(t *testing.T, f *watchFixture, name string, want int64) {
	t.Helper()
	m, err := f.tel.GetMetric(name)
	if err != nil {
		assert.Zero(t, want, "%s was never recorded", name)
		return
	}
	sum, ok := m.Data.(metricdata.Sum[int64])
	require.True(t, ok, "%s is not an int64 sum but %T", name, m.Data)
	var total int64
	for _, dp := range sum.DataPoints {
		total += dp.Value
	}
	assert.Equal(t, want, total, name)
}
