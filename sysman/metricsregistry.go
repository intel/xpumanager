//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"fmt"
	"log/slog"
	"maps"
	"slices"

	"go.opentelemetry.io/otel/metric"
)

type collector interface {
	getInstruments() []metric.Observable
	observeDevice(o metric.Observer, d *sysmanDevice)
}

type metricsRegistry struct {
	collectors []collector
}

func newMetricsRegistry(meter metric.Meter) (*metricsRegistry, error) {
	registry := &metricsRegistry{}

	for _, name := range slices.Sorted(maps.Keys(subsystems)) {
		s := subsystems[name]
		c, err := s.createCollector(meter)
		if err != nil {
			return nil, fmt.Errorf("failed to create collector for subsystem %s: %w", name, err)
		}
		registry.collectors = append(registry.collectors, c)
		slog.Info("registered sysman subsystem collector", "subsystem", name)
	}

	return registry, nil
}

func (r *metricsRegistry) getInstruments() []metric.Observable {
	instruments := []metric.Observable{}
	for _, c := range r.collectors {
		instruments = append(instruments, c.getInstruments()...)
	}
	return instruments
}

func (r *metricsRegistry) observe(o metric.Observer, d *deviceRegistry) {
	for _, dev := range d.devices {
		dev.observe(o, r)
	}
}
