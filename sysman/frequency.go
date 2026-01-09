//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"strings"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.uber.org/zap"

	l0sysman "github.com/intel/level-zero-go/sysman"
)

func init() {
	registerSubsystem("frequency", newFrequencyMetrics)
}

type sysmanFrequency struct {
	*l0sysman.Freq
	logger *zap.SugaredLogger

	state      sysmanFrequencyState
	attributes map[string]string

	// Aggregated metrics
	actual *aggregatedMetric[float64]
}

// sysmanFrequencyState holds the dynamic runtime state.
type sysmanFrequencyState struct {
	throttleReasonsSeen l0sysman.FreqThrottleReasonFlags
}

// frequencyMetrics is a throwaway struct to hold frequency metrics for one scrape cycle.
type frequencyMetrics struct {
	timestamp pcommon.Timestamp

	// TODO: make it possible to disable the debug metrics(?)
	speed   pmetric.Gauge
	limit   pmetric.Gauge
	request pmetric.Gauge
	samples pmetric.Gauge // debug
	status  pmetric.Sum
}

func newSysmanFrequency(name string, freq *l0sysman.Freq, device *sysmanDevice) (*sysmanFrequency, error) {
	props, err := freq.GetProperties()
	if err != nil {
		return nil, err
	}

	attrs := map[string]string{
		attrGpuSpeedType: strings.ToLower(props.Type.String()),
		attrSubdeviceId:  subDeviceIdString(props.OnSubdevice, props.SubdeviceId),
	}

	return &sysmanFrequency{
		Freq:       freq,
		logger:     device.logger,
		attributes: attrs,
		actual:     newAggregatedMetric[float64](device.aggregatedMetricsBufferSize, maxAggregatedMetricsReaders),
	}, nil
}

func (t *sysmanFrequency) addAttributes(attrs pcommon.Map, keys ...string) {
	addAttributes(attrs, t.attributes, keys...)
}

func newFrequencyMetrics(sm pmetric.ScopeMetrics, ts pcommon.Timestamp) scraper {
	return &frequencyMetrics{
		timestamp: ts,

		// Metrics
		// NOTE: hw.gpu.speed imitates hw.cpu.speed, but is not in OTel spec:
		// https://opentelemetry.io/docs/specs/semconv/hardware/gpu/
		speed:   newGauge(sm, "hw.gpu.speed", "Hz", "Current GPU frequency"),
		limit:   newGauge(sm, "hw.gpu.speed.limit", "Hz", "GPU frequency limit"),
		request: newGauge(sm, "hw.gpu.speed.request", "Hz", "Requested GPU frequency"),
		samples: newGauge(sm, "hw.gpu.speed.samples", "{count}", "Number of speed samples during the last collection interval"),
		status:  newUpDownCounter(sm, "hw.gpu.speed.status", "1", "GPU frequency status"),
	}
}

func (m *frequencyMetrics) scrapeDevice(dev *sysmanDevice) {
	for _, freq := range dev.frequency {
		// Helper functions for attribute handling
		with := func(key, value string) func(pcommon.Map) {
			return func(attrs pcommon.Map) {
				attrs.PutStr(key, value)
			}
		}
		newAttrs := func(opts ...func(pcommon.Map)) pcommon.Map {
			attrs := dev.pickAttributes(attrHwID, attrHwModel, attrHwName, attrHwSerialNumber, attrHwVendor)
			freq.addAttributes(attrs, attrGpuSpeedType, attrSubdeviceId)
			return attrs
		}

		// Handle instantaneous metrics
		if rang, err := freq.GetRange(); err != nil {
			freq.logger.Errorw("Failed to get frequency range", zap.Error(err), "attributes", freq.attributes)
		} else {
			if rang.Min >= 0 {
				attrs := newAttrs(with(attrHwLimitType, "min"))
				observeFloat64(m.limit, rang.Min*1e6, m.timestamp, attrs)
			}
			if rang.Max >= 0 {
				attrs := newAttrs(with(attrHwLimitType, "max"))
				observeFloat64(m.limit, rang.Max*1e6, m.timestamp, attrs)
			}
		}

		if state, err := freq.GetState(); err != nil {
			freq.logger.Errorw("Failed to get frequency state", zap.Error(err), "attributes", freq.attributes)
		} else {
			if state.Request >= 0 {
				attrs := newAttrs()
				observeFloat64(m.request, state.Request*1e6, m.timestamp, attrs)
			}

			stateOK := int64(1)
			if state.ThrottleReasons != 0 {
				stateOK = 0
			}
			attrs := newAttrs(with(attrHwState, "ok"))
			observeInt64(m.status, stateOK, m.timestamp, attrs)

			for reason := l0sysman.FreqThrottleReasonFlag(1); reason <= l0sysman.FREQ_THROTTLE_REASON_FLAG_HW_RANGE; reason <<= 1 {
				value := int64(0)
				if l0sysman.FreqThrottleReasonFlag(state.ThrottleReasons)&reason != 0 {
					value = 1
					freq.state.throttleReasonsSeen |= l0sysman.FreqThrottleReasonFlags(reason)
				}

				// Flags may be unset because the driver lacks support for this
				// throttle reason (does not know the status). Emit the metric
				// only once support is confirmed.
				if l0sysman.FreqThrottleReasonFlag(freq.state.throttleReasonsSeen)&reason != 0 {
					attrs := newAttrs(
						with(attrHwState, "throttled"),
						with(attrGpuSpeedThrottleReason, strings.ToLower(reason.String())))
					observeInt64(m.status, value, m.timestamp, attrs)
				}
			}
		}

		// Handle aggregated metrics
		actualStats := freq.actual.collect(0)
		if actualStats.samples > 0 {
			attrs := newAttrs(with(attrAggregation, "min"))
			observeFloat64(m.speed, actualStats.minValue*1e6, m.timestamp, attrs)

			attrs = newAttrs(with(attrAggregation, "max"))
			observeFloat64(m.speed, actualStats.maxValue*1e6, m.timestamp, attrs)

			attrs = newAttrs(with(attrAggregation, "avg"))
			observeFloat64(m.speed, actualStats.avgValue*1e6, m.timestamp, attrs)

			// Sample debug metrics
			attrs = newAttrs(with(attrSampleStatus, "collected"))
			observeInt64(m.samples, int64(actualStats.samples), m.timestamp, attrs)

			attrs = newAttrs(with(attrSampleStatus, "dropped"))
			observeInt64(m.samples, int64(actualStats.lostSamples), m.timestamp, attrs)
		}
		if actualStats.lostSamples > 0 {
			freq.logger.Debugw("Lost samples of actual frequency", "count", actualStats.lostSamples, "attributes", freq.attributes)
		}
	}
}

func (f *sysmanFrequency) pollAggregatedMetrics() {
	if state, err := f.GetState(); err != nil {
		f.logger.Errorw("Failed to get frequency state for aggregated metrics", zap.Error(err), "attributes", f.attributes)
	} else {
		f.actual.add(state.Actual)
	}
}
