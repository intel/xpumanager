//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"log/slog"
	"strings"

	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/metric"

	l0sysman "github.com/intel/level-zero-go/sysman"
)

func init() {
	registerSubsystem("frequency", newFrequencyMetrics)
}

type sysmanFrequency struct {
	*l0sysman.Freq

	state      sysmanFrequencyState
	attributes []attribute.KeyValue

	// Aggregated metrics
	actual *aggregatedMetric[float64]
}

// sysmanFrequencyState holds the dynamic runtime state.
type sysmanFrequencyState struct {
	throttleReasonsSeen l0sysman.FreqThrottleReasonFlags
}

type frequencyMetrics struct {
	minimum        metric.Float64ObservableGauge
	maximum        metric.Float64ObservableGauge
	request        metric.Float64ObservableGauge
	actualMin      metric.Float64ObservableGauge
	actualMax      metric.Float64ObservableGauge
	actualAvg      metric.Float64ObservableGauge
	throttleReason metric.Int64ObservableUpDownCounter
}

func newSysmanFrequency(freq *l0sysman.Freq, aggregatedMetricsBufferSize int) (*sysmanFrequency, error) {
	props, err := freq.GetProperties()
	if err != nil {
		return nil, err
	}
	attrs := []attribute.KeyValue{
		attribute.String("hw.gpu.frequency.type", strings.ToLower(props.Type.String())),
		attribute.String("hw.gpu.frequency.subdevice_id", subDeviceIdString(props.OnSubdevice, props.SubdeviceId)),
	}

	return &sysmanFrequency{
		Freq:       freq,
		attributes: attrs,
		actual:     newAggregatedMetric[float64](aggregatedMetricsBufferSize, 1),
	}, nil
}

func enumFrequency(d *l0sysman.Device, aggregatedMetricsBufferSize int) []*sysmanFrequency {
	freqs, err := d.EnumFrequencyDomains()
	if err != nil {
		slog.Error("Failed to enumerate frequency domains", "error", err)
		return nil
	}
	frequency := make([]*sysmanFrequency, len(freqs))
	for i, freq := range freqs {
		f, err := newSysmanFrequency(freq, aggregatedMetricsBufferSize)
		if err != nil {
			slog.Error("Failed to create sysman frequency domain", "error", err)
			continue
		}
		frequency[i] = f
	}
	return frequency
}

func newFrequencyMetrics(meter metric.Meter) (collector, error) {
	var err error
	m := &frequencyMetrics{}

	m.minimum, err = meter.Float64ObservableGauge(
		"hw.gpu.frequency.minimum",
		metric.WithDescription("Minimum GPU frequency"),
		metric.WithUnit("Hz"),
	)
	if err != nil {
		return nil, err
	}
	m.maximum, err = meter.Float64ObservableGauge(
		"hw.gpu.frequency.maximum",
		metric.WithDescription("Maximum GPU frequency"),
		metric.WithUnit("Hz"),
	)
	if err != nil {
		return nil, err
	}
	m.request, err = meter.Float64ObservableGauge(
		"hw.gpu.frequency.request",
		metric.WithDescription("Requested GPU frequency"),
		metric.WithUnit("Hz"),
	)
	if err != nil {
		return nil, err
	}
	m.actualMin, err = meter.Float64ObservableGauge(
		"hw.gpu.frequency.actual.min",
		metric.WithDescription("Actual GPU frequency minimum value during the last collection interval"),
		metric.WithUnit("Hz"),
	)
	if err != nil {
		return nil, err
	}
	m.actualMax, err = meter.Float64ObservableGauge(
		"hw.gpu.frequency.actual.max",
		metric.WithDescription("Actual GPU frequency maximum value during the last collection interval"),
		metric.WithUnit("Hz"),
	)
	if err != nil {
		return nil, err
	}
	m.actualAvg, err = meter.Float64ObservableGauge(
		"hw.gpu.frequency.actual.avg",
		metric.WithDescription("Actual GPU frequency average value during the last collection interval"),
		metric.WithUnit("Hz"),
	)
	if err != nil {
		return nil, err
	}
	m.throttleReason, err = meter.Int64ObservableUpDownCounter(
		"hw.gpu.frequency.throttle_reason",
		metric.WithDescription("GPU frequency throttle reason"),
		metric.WithUnit("1"),
	)
	if err != nil {
		return nil, err
	}

	return m, nil
}

func (m *frequencyMetrics) getInstruments() []metric.Observable {
	return []metric.Observable{
		m.minimum,
		m.maximum,
		m.request,
		m.actualMin,
		m.actualMax,
		m.actualAvg,
		m.throttleReason,
	}
}

func (m *frequencyMetrics) observeDevice(o metric.Observer, dev *sysmanDevice) {
	for _, freq := range dev.frequency {
		attrs := append(dev.attributes, freq.attributes...)
		opt := metric.WithAttributes(attrs...)

		if rang, err := freq.GetRange(); err != nil {
			slog.Error("Failed to get frequency range", "error", err, attrsToSlog(attrs))
		} else {
			if rang.Min >= 0 {
				o.ObserveFloat64(m.minimum, rang.Min*1e6, opt)
			}
			if rang.Max >= 0 {
				o.ObserveFloat64(m.maximum, rang.Max*1e6, opt)
			}
		}

		if state, err := freq.GetState(); err != nil {
			slog.Error("Failed to get frequency state", "error", err, attrsToSlog(attrs))
		} else {
			if state.Request >= 0 {
				o.ObserveFloat64(m.request, state.Request*1e6, opt)
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
					reasonAttr := attribute.String("hw.gpu.frequency.throttle_reason.type", strings.ToLower(reason.String()))
					o.ObserveInt64(m.throttleReason, value, metric.WithAttributes(append(attrs, reasonAttr)...))
				}
			}
		}

		// Handle aggregated metrics
		actualStats := freq.actual.collect(0)
		if actualStats.samples > 0 {
			o.ObserveFloat64(m.actualMin, actualStats.minValue*1e6, opt)
			o.ObserveFloat64(m.actualMax, actualStats.maxValue*1e6, opt)
			o.ObserveFloat64(m.actualAvg, actualStats.avgValue*1e6, opt)
		}
		if actualStats.lostSamples > 0 {
			slog.Debug("Lost samples of actual frequency", "count", actualStats.lostSamples, attrsToSlog(attrs))
		}
	}
}

func (f *sysmanFrequency) pollAggregatedMetrics() {
	if state, err := f.GetState(); err != nil {
		slog.Error("Failed to get frequency state for aggregated metrics", "error", err, attrsToSlog(f.attributes))
	} else {
		f.actual.add(state.Actual)
	}
}
