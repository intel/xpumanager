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
	// TODO: make it possible to disable the debug metrics(?)
	speed   gauge
	limit   gauge
	request gauge
	samples gauge // debug
	status  sum
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

func newFrequencyMetrics(sm pmetric.ScopeMetrics, _ *commonMetrics, ts pcommon.Timestamp) scraper {
	return &frequencyMetrics{
		// NOTE: hw.gpu.speed imitates hw.cpu.speed, but is not in OTel spec:
		// https://opentelemetry.io/docs/specs/semconv/hardware/gpu/
		speed:   newGauge(sm, ts, "hw.gpu.speed", "Hz", "Current GPU frequency"),
		limit:   newGauge(sm, ts, "hw.gpu.speed.limit", "Hz", "GPU frequency limit"),
		request: newGauge(sm, ts, "hw.gpu.speed.request", "Hz", "Requested GPU frequency"),
		samples: newGauge(sm, ts, "hw.gpu.speed.samples", "{count}", "Number of speed samples during the last collection interval"),
		status:  newUpDownCounter(sm, ts, "hw.gpu.speed.status", "1", "GPU frequency status"),
	}
}

func (m *frequencyMetrics) scrapeDevice(dev *sysmanDevice) {
	for _, freq := range dev.frequency {
		// Helper functions for attribute handling
		baseAttrs := func() pcommon.Map {
			attrs := dev.pickAttributes(attrHwID, attrHwModel, attrHwName, attrHwSerialNumber, attrHwVendor)
			freq.addAttributes(attrs, attrGpuSpeedType, attrSubdeviceId)
			return attrs
		}

		// Handle instantaneous metrics
		if rang, err := freq.GetRange(); err != nil {
			freq.logger.Errorw("Failed to get frequency range", zap.Error(err), "attributes", freq.attributes)
		} else {
			if rang.Min >= 0 {
				attrs := baseAttrs()
				attrs.PutStr(attrHwLimitType, "min")
				m.limit.observeFloat64(rang.Min*1e6, attrs)
			}
			if rang.Max >= 0 {
				attrs := baseAttrs()
				attrs.PutStr(attrHwLimitType, "max")
				m.limit.observeFloat64(rang.Max*1e6, attrs)
			}
		}

		if state, err := freq.GetState(); err != nil {
			freq.logger.Errorw("Failed to get frequency state", zap.Error(err), "attributes", freq.attributes)
		} else {
			if state.Request >= 0 {
				attrs := baseAttrs()
				m.request.observeFloat64(state.Request*1e6, attrs)
			}

			stateOK := int64(1)
			if state.ThrottleReasons != 0 {
				stateOK = 0
			}
			attrs := baseAttrs()
			attrs.PutStr(attrHwState, "ok")
			m.status.observeInt64(stateOK, attrs)

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
					attrs := baseAttrs()
					attrs.PutStr(attrHwState, "throttled")
					attrs.PutStr(attrGpuSpeedThrottleReason, strings.ToLower(reason.String()))
					m.status.observeInt64(value, attrs)
				}
			}
		}

		// Handle aggregated metrics
		actualStats := freq.actual.collect(0)
		if actualStats.samples > 0 {
			attrs := baseAttrs()
			attrs.PutStr(attrAggregation, "min")
			m.speed.observeFloat64(actualStats.minValue*1e6, attrs)

			attrs = baseAttrs()
			attrs.PutStr(attrAggregation, "max")
			m.speed.observeFloat64(actualStats.maxValue*1e6, attrs)

			attrs = baseAttrs()
			attrs.PutStr(attrAggregation, "avg")
			m.speed.observeFloat64(actualStats.avgValue*1e6, attrs)

			// Sample debug metrics
			attrs = baseAttrs()
			attrs.PutStr(attrSampleStatus, "collected")
			m.samples.observeInt64(int64(actualStats.samples), attrs)

			attrs = baseAttrs()
			attrs.PutStr(attrSampleStatus, "dropped")
			m.samples.observeInt64(int64(actualStats.lostSamples), attrs)
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
