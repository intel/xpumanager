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
	attributes []attribute.KeyValue
}

type frequencyMetrics struct {
	minimum metric.Float64ObservableGauge
	maximum metric.Float64ObservableGauge
	request metric.Float64ObservableGauge
	actual  metric.Float64ObservableGauge
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

	return m, nil
}

func (m *frequencyMetrics) getInstruments() []metric.Observable {
	return []metric.Observable{
		m.minimum,
		m.maximum,
		m.request,
		m.actual,
	}
}

func (m *frequencyMetrics) observeDevice(o metric.Observer, dev *sysmanDevice) {
	for _, freq := range dev.frequency {
		attrs := append(dev.attributes, freq.attributes...)
		opt := metric.WithAttributes(attrs...)

		if rang, err := freq.GetRange(); err != nil {
			slog.Error("Failed to get frequency range", "error", err, attrsToSlog(attrs))
		} else {
			o.ObserveFloat64(m.minimum, rang.Min*1000000, opt)
			o.ObserveFloat64(m.maximum, rang.Max*1000000, opt)
		}

		if state, err := freq.GetState(); err != nil {
			slog.Error("Failed to get frequency state", "error", err, attrsToSlog(attrs))
		} else {
			o.ObserveFloat64(m.request, state.Request*1000000, opt)
			o.ObserveFloat64(m.actual, state.Actual*1000000, opt)
		}
	}
}
