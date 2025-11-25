//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"go.opentelemetry.io/otel/metric"
)

type metricsRegistry struct {
	frequency   *frequencyMetrics
	memory      *memoryMetrics
	temperature *temperatureMetrics
}

func newMetricsRegistry(meter metric.Meter) (*metricsRegistry, error) {
	var err error
	registry := &metricsRegistry{}

	registry.frequency, err = newFrequencyMetrics(meter)
	if err != nil {
		return nil, err
	}

	registry.memory, err = newMemoryMetrics(meter)
	if err != nil {
		return nil, err
	}

	registry.temperature, err = newTemperatureMetrics(meter)
	if err != nil {
		return nil, err
	}
	return registry, nil
}

func (r *metricsRegistry) getInstruments() []metric.Observable {
	instruments := []metric.Observable{}
	instruments = append(instruments, r.frequency.getInstruments()...)
	instruments = append(instruments, r.memory.getInstruments()...)
	instruments = append(instruments, r.temperature.getInstruments()...)
	return instruments
}

func (r *metricsRegistry) observe(o metric.Observer, d *deviceRegistry) {
	for _, dev := range d.devices {
		dev.observe(o, r)
	}
}
