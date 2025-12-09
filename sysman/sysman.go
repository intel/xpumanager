//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"context"

	"go.opentelemetry.io/otel/metric"

	l0sysman "github.com/intel/level-zero-go/sysman"
)

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
	metrics *metricsRegistry
	devices *deviceRegistry
}

// New creates and initializes a new Sysman instance.
func New() (*sysman, error) {
	s := &sysman{}
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
	s.devices, err = newDeviceRegistry()
	if err != nil {
		return err
	}
	return nil
}

// RegisterMetrics registers Sysman metrics callbacks with the provided OTEL meter.
func (s *sysman) RegisterMetrics(meter metric.Meter) error {
	if err := s.initMetrics(meter); err != nil {
		return err
	}
	_, err := meter.RegisterCallback(
		func(ctx context.Context, o metric.Observer) error {
			s.metrics.observe(o, s.devices)
			return nil
		},
		s.metrics.getInstruments()...,
	)
	return err
}

func (s *sysman) PollAggregatedMetrics() {
	s.devices.pollAggregatedMetrics()
}

func (s *sysman) initMetrics(meter metric.Meter) error {
	var err error
	s.metrics, err = newMetricsRegistry(meter)
	if err != nil {
		return err
	}

	return nil
}
