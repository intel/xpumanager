//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"context"
	"fmt"

	"go.opentelemetry.io/otel/metric"

	l0sysman "github.com/intel/level-zero-go/sysman"
)

const (
	// Maximum number of independent metric readers for aggregated metrics.
	maxAggregatedMetricsReaders = 2
)

// Handle is the interface for Sysman functionality.
type Handle interface {
	RegisterMetrics(meter metric.Meter) error
	PollAggregatedMetrics()
}

type createCollectorFunc func(metric.Meter) (collector, error)

type subsystem struct {
	createCollector func(metric.Meter) (collector, error)
}

var subsystems = map[string]subsystem{}

func registerSubsystem(name string, create createCollectorFunc) {
	if _, exists := subsystems[name]; exists {
		panic("subsystem already registered: " + name)
	}
	subsystems[name] = subsystem{
		createCollector: create,
	}
}

type sysman struct {
	aggregatedMetricsBufferSize int
	metrics                     []*metricsRegistry
	devices                     *deviceRegistry
}

// New creates and initializes a new Sysman instance.
func New(aggregatedMetricsBufferSize int) (Handle, error) {
	s := &sysman{
		aggregatedMetricsBufferSize: aggregatedMetricsBufferSize,
	}
	if err := s.init(); err != nil {
		return nil, err
	}
	return s, nil
}

func (s *sysman) init() error {
	if err := l0sysman.Init(0); err != nil {
		return err
	}

	var err error
	s.devices, err = newDeviceRegistry(s.aggregatedMetricsBufferSize)
	if err != nil {
		return err
	}
	return nil
}

// RegisterMetrics registers Sysman metrics callbacks with the provided OTEL meter.
func (s *sysman) RegisterMetrics(meter metric.Meter) error {
	idx := len(s.metrics)
	if idx >= maxAggregatedMetricsReaders {
		return fmt.Errorf("maximum number of aggregated metric readers (%d) exceeded", maxAggregatedMetricsReaders)
	}

	m, err := newMetricsRegistry(meter)
	if err != nil {
		return err
	}
	s.metrics = append(s.metrics, m)

	_, err = meter.RegisterCallback(
		func(ctx context.Context, o metric.Observer) error {
			m.observe(o, s.devices, idx)
			return nil
		},
		m.getInstruments()...,
	)
	return err
}

func (s *sysman) PollAggregatedMetrics() {
	s.devices.pollAggregatedMetrics()
}
