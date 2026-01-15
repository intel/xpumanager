//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"maps"
	"slices"
	"time"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.uber.org/zap"
)

const (
	// Standard semantic convention attributes:
	// https://opentelemetry.io/docs/specs/semconv/registry/attributes/hardware
	attrHwID             = "hw.id"
	attrHwLimitType      = "hw.limit_type"
	attrHwModel          = "hw.model"
	attrHwName           = "hw.name"
	attrHwParent         = "hw.parent"
	attrHwSerialNumber   = "hw.serial_number"
	attrHwSensorLocation = "hw.sensor_location"
	attrHwState          = "hw.state"
	attrHwType           = "hw.type"
	attrHwVendor         = "hw.vendor"
	attrMemoryType       = "hw.memory.type"
	// NOTE: Non-standard (not specified in semantic conventions) and vendor-specific attributes
	attrAggregation            = "aggregation"
	attrStatistic              = "statistic"
	attrSubdeviceId            = "com.intel.gpu.subdevice_id"
	attrGpuSpeedThrottleReason = "com.intel.gpu.speed.throttle_reason"
	attrGpuSpeedType           = "hw.gpu.speed.type"
	attrSampleStatus           = "sample.status"
)

type scraper interface {
	scrapeDevice(*sysmanDevice)
}

type metricsRegistry struct {
	logger   *zap.SugaredLogger
	scrapers []subsystem
}

// commonMetrics holds common hardware metrics used across subsystems.
type commonMetrics struct {
	status sum
}

func newMetricsRegistry(logger *zap.SugaredLogger) (*metricsRegistry, error) {
	registry := &metricsRegistry{logger: logger}

	for _, name := range slices.Sorted(maps.Keys(subsystems)) {
		s := subsystems[name]
		registry.scrapers = append(registry.scrapers, s)
		logger.Infow("registered sysman subsystem scraper", "subsystem", name)
	}

	return registry, nil
}

func (r *metricsRegistry) scrape(d *deviceRegistry) pmetric.Metrics {
	metrics := pmetric.NewMetrics()
	rm := metrics.ResourceMetrics().AppendEmpty()
	sm := rm.ScopeMetrics().AppendEmpty()
	sm.Scope().SetName("sysman")
	sm.Scope().SetVersion("0.0.0")

	// Create metric scrapers
	ts := pcommon.NewTimestampFromTime(time.Now())
	gm := newGenericMetrics(sm, ts)

	scrapers := make([]scraper, len(r.scrapers))
	for i, s := range r.scrapers {
		scrapers[i] = s.createScraper(sm, gm, ts)
	}

	// Scrape devices
	for _, dev := range d.devices {
		for _, scraper := range scrapers {
			scraper.scrapeDevice(dev)
		}
	}

	return metrics
}

func newGenericMetrics(sm pmetric.ScopeMetrics, ts pcommon.Timestamp) *commonMetrics {
	return &commonMetrics{
		status: newUpDownCounter(sm, ts, "hw.status", "1", "The operational status of the hardware component"),
	}
}

type gauge struct {
	pmetric.Gauge
	timestamp pcommon.Timestamp
}

type sum struct {
	pmetric.Sum
	timestamp pcommon.Timestamp
}

func newGauge(sm pmetric.ScopeMetrics, ts pcommon.Timestamp, name, unit, description string) gauge {
	m := sm.Metrics().AppendEmpty()
	m.SetName(name)
	m.SetUnit(unit)
	m.SetDescription(description)
	return gauge{
		Gauge:     m.SetEmptyGauge(),
		timestamp: ts,
	}
}

func newUpDownCounter(sm pmetric.ScopeMetrics, ts pcommon.Timestamp, name, unit, description string) sum {
	m := sm.Metrics().AppendEmpty()
	m.SetName(name)
	m.SetUnit(unit)
	m.SetDescription(description)

	s := m.SetEmptySum()
	s.SetAggregationTemporality(pmetric.AggregationTemporalityCumulative)
	s.SetIsMonotonic(false)

	return sum{
		Sum:       s,
		timestamp: ts,
	}
}

func (g *gauge) observeInt64(value int64, attrs pcommon.Map) {
	observeInt64(g.Gauge, value, g.timestamp, attrs)
}

func (g *gauge) observeFloat64(value float64, attrs pcommon.Map) {
	observeFloat64(g.Gauge, value, g.timestamp, attrs)
}

func (s *sum) observeInt64(value int64, attrs pcommon.Map) {
	observeInt64(s.Sum, value, s.timestamp, attrs)
}

func observeInt64[T interface {
	DataPoints() pmetric.NumberDataPointSlice
}](m T, value int64, ts pcommon.Timestamp, attrs pcommon.Map) {
	dp := m.DataPoints().AppendEmpty()
	dp.SetIntValue(value)
	dp.SetTimestamp(ts)
	attrs.CopyTo(dp.Attributes())
}

func observeFloat64[T interface {
	DataPoints() pmetric.NumberDataPointSlice
}](m T, value float64, ts pcommon.Timestamp, attrs pcommon.Map) {
	dp := m.DataPoints().AppendEmpty()
	dp.SetDoubleValue(value)
	dp.SetTimestamp(ts)
	attrs.CopyTo(dp.Attributes())
}

func pickAttributes(attrs map[string]string, keys ...string) pcommon.Map {
	a := pcommon.NewMap()
	addAttributes(a, attrs, keys...)
	return a
}

func addAttributes(dst pcommon.Map, src map[string]string, keys ...string) {
	for _, key := range keys {
		val, ok := src[key]
		if !ok {
			panic("attribute key not found: " + key)
		}
		dst.PutStr(key, val)
	}
}
