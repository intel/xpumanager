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
	registerSubsystem("temperature", newTemperatureMetrics)
}

type sysmanTemperature struct {
	*levelzero.ZesTemp
	attributes []attribute.KeyValue
}

type temperatureMetrics struct {
	current metric.Float64ObservableGauge
}

func newSysmanTemperature(temp *levelzero.ZesTemp) (*sysmanTemperature, error) {
	props, err := temp.GetProperties()
	if err != nil {
		return nil, err
	}
	attrs := []attribute.KeyValue{
		attribute.String("hw.gpu.temperature.type", strings.ToLower(props.Type.String())),
		attribute.String("hw.gpu.temperature.subdevice_id", subDeviceIdString(props.OnSubdevice, props.SubdeviceId)),
	}
	return &sysmanTemperature{
		ZesTemp:    temp,
		attributes: attrs,
	}, nil
}

func enumTemperature(d *levelzero.ZeDevice) []*sysmanTemperature {
	temps, err := d.EnumTemperatureSensors()
	if err != nil {
		slog.Error("Failed to enumerate temperature sensors", "error", err)
		return nil
	}
	temperature := make([]*sysmanTemperature, len(temps))
	for i, temp := range temps {
		t, err := newSysmanTemperature(temp)
		if err != nil {
			slog.Error("Failed to create sysman temperature sensor", "error", err)
			continue
		}
		temperature[i] = t
	}
	return temperature
}

func newTemperatureMetrics(meter metric.Meter) (collector, error) {
	var err error
	m := &temperatureMetrics{}
	m.current, err = meter.Float64ObservableGauge(
		"hw.gpu.temperature.current",
		metric.WithDescription("Current GPU temperature"),
		metric.WithUnit("Celsius"),
	)
	if err != nil {
		return nil, err
	}
	return m, nil
}

func (m *temperatureMetrics) getInstruments() []metric.Observable {
	return []metric.Observable{
		m.current,
	}
}

func (m *temperatureMetrics) observeDevice(o metric.Observer, dev *sysmanDevice) {
	for _, temp := range dev.temperature {
		attrs := append(dev.attributes, temp.attributes...)
		opt := metric.WithAttributes(attrs...)
		if current, err := temp.GetState(); err != nil {
			slog.Error("Failed to get temperature state", "error", err, attrsToSlog(attrs))
		} else {
			o.ObserveFloat64(m.current, current, opt)
		}
	}
}
