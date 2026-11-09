//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"reflect"
	"slices"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.uber.org/zap"

	"github.com/intel/xpumanager/xpumd/receiver/intelxpu/sysman/internal/metadata"
)

// metricNames returns the names of all metrics.
func metricNames() []string {
	names := []string{}
	t := reflect.TypeFor[metadata.MetricsConfig]()
	for i := range t.NumField() {
		names = append(names, t.Field(i).Tag.Get("mapstructure"))
	}
	return names
}

// metricsConfig returns a config with only the given metrics enabled.
func metricsConfig(t *testing.T, enabled ...string) *metadata.MetricsConfig {
	require.Subset(t, metricNames(), enabled)
	c := metadata.DefaultMetricsConfig()
	v := reflect.ValueOf(&c).Elem()
	for i := range v.NumField() {
		name := v.Type().Field(i).Tag.Get("mapstructure")
		v.Field(i).FieldByName("Enabled").SetBool(slices.Contains(enabled, name))
	}
	return &c
}

// TestMetricNeeds checks that every metric is needed by some subsystem function.
func TestMetricNeeds(t *testing.T) {
	for _, name := range metricNames() {
		if name == "hw.gpu.info" {
			continue // reported by the device itself, regardless of the subsystems
		}
		c := metricsConfig(t, name)
		needed := false
		for _, s := range subsystems {
			needed = needed || s.isNeeded(c)
		}
		assert.True(t, needed, "metric %s not needed by any subsystem function", name)
	}

	c := metricsConfig(t)
	for name, s := range subsystems {
		assert.False(t, s.isNeeded(c), "subsystem %s needed without enabled metrics", name)
	}
}

func TestNeedSampling(t *testing.T) {
	tests := []struct {
		metrics []string
		needed  bool
	}{
		{metrics: []string{"hw.frequency"}, needed: true},
		{metrics: []string{"hw.frequency.samples"}, needed: true},
		// throttle reasons seen between scrapes
		{metrics: []string{"hw.frequency.throttle_status"}, needed: true},
		{metrics: []string{"hw.status"}, needed: true},
		{metrics: []string{"hw.frequency.limit", "hw.frequency.request"}, needed: false},
		{metrics: []string{"hw.power", "hw.memory.usage", "hw.gpu.utilization"}, needed: false},
		{metrics: nil, needed: false},
	}
	for _, tt := range tests {
		cfg := &Config{}
		cfg.Metrics = *metricsConfig(t, tt.metrics...)
		assert.Equal(t, tt.needed, needSampling(cfg), "metrics: %v", tt.metrics)
	}
	cfg := defaultConfig().(*Config)
	assert.True(t, needSampling(cfg))

	// only the frequency subsystem needs sampling
	disabled := false
	cfg.Subsystems = map[string]SubsystemConfig{"frequency": {Enabled: &disabled}}
	assert.False(t, needSampling(cfg))
}

// fakeInstance is a subsystem instance recording the calls of its functions.
type fakeInstance struct {
	name string
	// available tells whether initB succeeds, and whether sampling continues
	available bool
	calls     *[]string
}

func (f *fakeInstance) record(call string) { *f.calls = append(*f.calls, f.name+":"+call) }

func (f *fakeInstance) scrapeA(*metadata.MetricsBuilder, pcommon.Timestamp) bool {
	f.record("scrapeA")
	return true
}
func (f *fakeInstance) scrapeB(*metadata.MetricsBuilder, pcommon.Timestamp) bool {
	f.record("scrapeB")
	return true
}
func (f *fakeInstance) initB() bool   { f.record("initB"); return f.available }
func (f *fakeInstance) sampleB() bool { f.record("sampleB"); return f.available }

func TestSubsystemSetup(t *testing.T) {
	needA := func(c *metadata.MetricsConfig) bool { return c.HwPower.Enabled }
	needB := func(c *metadata.MetricsConfig) bool { return c.HwEnergy.Enabled }

	calls := []string{}
	enum := func(*device) []*fakeInstance {
		calls = append(calls, "enum")
		return []*fakeInstance{{name: "x", available: true, calls: &calls}, {name: "y", calls: &calls}}
	}
	s := newSubsystem("fake", enum,
		onScrape((*fakeInstance).scrapeA, needA),
		onScrape((*fakeInstance).scrapeB, needB).withInit((*fakeInstance).initB),
		onSampling((*fakeInstance).sampleB, needB),
	)
	assert.False(t, s.isNeeded(metricsConfig(t, "hw.temperature")))
	assert.True(t, s.isSamplingNeeded(metricsConfig(t, "hw.energy")))
	assert.False(t, s.isSamplingNeeded(metricsConfig(t, "hw.power")))

	newDevice := func(metrics ...string) *device {
		return &device{logger: zap.NewNop().Sugar(), deviceConfig: deviceConfig{metrics: metricsConfig(t, metrics...)}}
	}
	run := func(d *device) []string {
		calls = calls[:0]
		for _, scrape := range d.scrapeFuncs {
			scrape(nil, 0)
		}
		for _, sampleFn := range d.samplingFuncs {
			sampleFn()
		}
		return calls
	}

	// nothing needed, thus instances not enumerated
	d := newDevice("hw.temperature")
	s.setup(d)
	assert.Empty(t, calls)
	assert.Empty(t, run(d))

	// B not needed, thus its init not called
	d = newDevice("hw.power")
	s.setup(d)
	assert.Equal(t, []string{"enum"}, calls)
	assert.Equal(t, []string{"x:scrapeA", "y:scrapeA"}, run(d))

	// B init fails for y
	calls = calls[:0]
	d = newDevice("hw.power", "hw.energy")
	s.setup(d)
	assert.Equal(t, []string{"enum", "x:initB", "y:initB"}, calls)
	assert.Equal(t, []string{"x:scrapeA", "x:scrapeB", "y:scrapeA", "x:sampleB", "y:sampleB"}, run(d))
	// sampleB returned false for y, thus not called again
	assert.Equal(t, []string{"x:scrapeA", "x:scrapeB", "y:scrapeA", "x:sampleB"}, run(d))
}
