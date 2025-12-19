//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"context"
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
	attributes pcommon.Map

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
	minimum           pmetric.Gauge
	maximum           pmetric.Gauge
	request           pmetric.Gauge
	actualMin         pmetric.Gauge
	actualMax         pmetric.Gauge
	actualAvg         pmetric.Gauge
	actualSamples     pmetric.Gauge // debug
	actualLostSamples pmetric.Gauge // debug
	throttleReason    pmetric.Sum
}

func newSysmanFrequency(freq *l0sysman.Freq, logger *zap.SugaredLogger, extraAttrs pcommon.Map, aggregatedMetricsBufferSize int) (*sysmanFrequency, error) {
	props, err := freq.GetProperties()
	if err != nil {
		return nil, err
	}
	attrs := pcommon.NewMap()
	extraAttrs.CopyTo(attrs)
	attrs.PutStr("hw.gpu.frequency.type", strings.ToLower(props.Type.String()))
	attrs.PutStr("hw.gpu.frequency.subdevice_id", subDeviceIdString(props.OnSubdevice, props.SubdeviceId))

	return &sysmanFrequency{
		Freq:       freq,
		logger:     logger,
		attributes: attrs,
		actual:     newAggregatedMetric[float64](aggregatedMetricsBufferSize, maxAggregatedMetricsReaders),
	}, nil
}

func newFrequencyMetrics(sm pmetric.ScopeMetrics, ts pcommon.Timestamp) scraper {
	return &frequencyMetrics{
		timestamp: ts,

		// Metrics
		minimum:           newGauge(sm, "hw.gpu.frequency.minimum", "Hz", "Minimum GPU frequency"),
		maximum:           newGauge(sm, "hw.gpu.frequency.maximum", "Hz", "Maximum GPU frequency"),
		request:           newGauge(sm, "hw.gpu.frequency.request", "Hz", "Requested GPU frequency"),
		actualMin:         newGauge(sm, "hw.gpu.frequency.actual.min", "Hz", "Actual GPU frequency minimum value during the last collection interval"),
		actualMax:         newGauge(sm, "hw.gpu.frequency.actual.max", "Hz", "Actual GPU frequency maximum value during the last collection interval"),
		actualAvg:         newGauge(sm, "hw.gpu.frequency.actual.avg", "Hz", "Actual GPU frequency average value during the last collection interval"),
		actualSamples:     newGauge(sm, "hw.gpu.frequency.actual.samples", "{count}", "Number of samples used to compute the actual GPU frequency statistics during the last collection interval"),
		actualLostSamples: newGauge(sm, "hw.gpu.frequency.actual.samples_lost", "{count}", "Number of the lost actual GPU frequency aggregation samples (from the beginning of the last collection interval)"),
		throttleReason:    newUpDownCounter(sm, "hw.gpu.frequency.throttle_reason", "1", "GPU frequency throttle reason"),
	}
}

func (m *frequencyMetrics) scrapeDevice(ctx context.Context, dev *sysmanDevice) {
	for _, freq := range dev.frequency {
		if rang, err := freq.GetRange(); err != nil {
			freq.logger.Errorw("Failed to get frequency range", zap.Error(err), "attributes", freq.attributes.AsRaw())
		} else {
			if rang.Min >= 0 {
				observeFloat64(m.minimum, rang.Min*1e6, m.timestamp, freq.attributes)
			}
			if rang.Max >= 0 {
				observeFloat64(m.maximum, rang.Max*1e6, m.timestamp, freq.attributes)
			}
		}

		if state, err := freq.GetState(); err != nil {
			freq.logger.Errorw("Failed to get frequency state", zap.Error(err), "attributes", freq.attributes.AsRaw())
		} else {
			if state.Request >= 0 {
				observeFloat64(m.request, state.Request*1e6, m.timestamp, freq.attributes)
			}

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
					reasonAttr := pcommon.NewMap()
					freq.attributes.CopyTo(reasonAttr)
					reasonAttr.PutStr("hw.gpu.frequency.throttle_reason.type", strings.ToLower(reason.String()))
					observeInt64(m.throttleReason, value, m.timestamp, reasonAttr)
				}
			}
		}

		// Handle aggregated metrics
		actualStats := freq.actual.collect(0)
		if actualStats.samples > 0 {
			observeFloat64(m.actualMin, actualStats.minValue*1e6, m.timestamp, freq.attributes)
			observeFloat64(m.actualMax, actualStats.maxValue*1e6, m.timestamp, freq.attributes)
			observeFloat64(m.actualAvg, actualStats.avgValue*1e6, m.timestamp, freq.attributes)
			observeInt64(m.actualSamples, int64(actualStats.samples), m.timestamp, freq.attributes)
			observeInt64(m.actualLostSamples, int64(actualStats.lostSamples), m.timestamp, freq.attributes)
		}
		if actualStats.lostSamples > 0 {
			freq.logger.Debugw("Lost samples of actual frequency", "count", actualStats.lostSamples, "attributes", freq.attributes.AsRaw())
		}
	}
}

func (f *sysmanFrequency) pollAggregatedMetrics() {
	if state, err := f.GetState(); err != nil {
		f.logger.Errorw("Failed to get frequency state for aggregated metrics", zap.Error(err), "attributes", f.attributes.AsRaw())
	} else {
		f.actual.add(state.Actual)
	}
}
