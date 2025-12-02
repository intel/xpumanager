//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"log/slog"
	"strings"

	"github.com/intel/level-zero-go/levelzero"
	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/metric"
)

func init() {
	registerSubsystem("frequency", newFrequencyMetrics)
}

type sysmanFrequency struct {
	*levelzero.ZesFreq

	state      sysmanFrequencyState
	attributes []attribute.KeyValue
}

// sysmanFrequencyState holds the dynamic runtime state.
type sysmanFrequencyState struct {
	throttleReasonsSeen levelzero.ZesFreqThrottleReasonFlags
}

type frequencyMetrics struct {
	minimum        metric.Float64ObservableGauge
	maximum        metric.Float64ObservableGauge
	request        metric.Float64ObservableGauge
	actual         metric.Float64ObservableGauge
	throttleReason metric.Int64ObservableUpDownCounter
}

func newSysmanFrequency(freq *levelzero.ZesFreq) (*sysmanFrequency, error) {
	props, err := freq.GetProperties()
	if err != nil {
		return nil, err
	}
	attrs := []attribute.KeyValue{
		attribute.String("hw.gpu.frequency.type", strings.ToLower(props.Type.String())),
		attribute.String("hw.gpu.frequency.subdevice_id", subDeviceIdString(props.OnSubdevice, props.SubdeviceId)),
	}

	return &sysmanFrequency{
		ZesFreq:    freq,
		attributes: attrs,
	}, nil
}

func enumFrequency(d *levelzero.ZeDevice) []*sysmanFrequency {
	freqs, err := d.EnumFrequencyDomains()
	if err != nil {
		slog.Error("Failed to enumerate frequency domains", "error", err)
		return nil
	}
	frequency := make([]*sysmanFrequency, len(freqs))
	for i, freq := range freqs {
		f, err := newSysmanFrequency(freq)
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
	m.actual, err = meter.Float64ObservableGauge(
		"hw.gpu.frequency.actual",
		metric.WithDescription("Actual GPU frequency"),
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
		m.actual,
		m.throttleReason,
	}
}

func (m *frequencyMetrics) observeDevice(o metric.Observer, dev *sysmanDevice) {
	for _, freq := range dev.frequency {
		attrs := append(dev.attributes, freq.attributes...)
		opt := metric.WithAttributes(attrs...)

		if rang, err := freq.GetRange(); err != nil {
			slog.Error("Failed to get frequency range", "error", err)
		} else {
			if rang.Min >= 0 {
				o.ObserveFloat64(m.minimum, rang.Min*1e6, opt)
			}
			if rang.Max >= 0 {
				o.ObserveFloat64(m.maximum, rang.Max*1e6, opt)
			}
		}

		if state, err := freq.GetState(); err != nil {
			slog.Error("Failed to get frequency state", "error", err)
		} else {
			if state.Request >= 0 {
				o.ObserveFloat64(m.request, state.Request*1e6, opt)
			}
			if state.Actual >= 0 {
				o.ObserveFloat64(m.actual, state.Actual*1e6, opt)
			}

			for reason := levelzero.ZesFreqThrottleReasonFlag(1); reason <= levelzero.ZES_FREQ_THROTTLE_REASON_FLAG_HW_RANGE; reason <<= 1 {
				value := int64(0)
				if levelzero.ZesFreqThrottleReasonFlag(state.ThrottleReasons)&reason != 0 {
					value = 1
					freq.state.throttleReasonsSeen |= levelzero.ZesFreqThrottleReasonFlags(reason)
				}

				// Flags may be unset because the driver lacks support for this
				// throttle reason (does not know the status). Emit the metric
				// only once support is confirmed.
				if levelzero.ZesFreqThrottleReasonFlag(freq.state.throttleReasonsSeen)&reason != 0 {
					reasonAttr := attribute.String("hw.gpu.frequency.throttle_reason.type", strings.ToLower(reason.String()))
					o.ObserveInt64(m.throttleReason, value, metric.WithAttributes(append(attrs, reasonAttr)...))
				}
			}
		}
	}
}
