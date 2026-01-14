//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.uber.org/zap"

	l0sysman "github.com/intel/level-zero-go/sysman"
)

const (
	// Maximum number of independent metric readers for aggregated metrics.
	maxAggregatedMetricsReaders = 1
)

// Handle is the interface for Sysman functionality.
type Handle interface {
	PollAggregatedMetrics()
	Scrape() pmetric.Metrics
}

type createScraperFunc func(pmetric.ScopeMetrics, pcommon.Timestamp) scraper

type subsystem struct {
	createScraper createScraperFunc
}

var subsystems = map[string]subsystem{}

func registerSubsystem(name string, create createScraperFunc) {
	if _, exists := subsystems[name]; exists {
		panic("subsystem already registered: " + name)
	}
	subsystems[name] = subsystem{
		createScraper: create,
	}
}

type sysman struct {
	metrics *metricsRegistry
	devices *deviceRegistry
}

// New creates and initializes a new Sysman instance.
func New(logger *zap.SugaredLogger, aggregatedMetricsBufferSize int) (Handle, error) {
	s := &sysman{}

	err := l0sysman.Init(0)
	if err != nil {
		return nil, err
	}

	s.devices, err = newDeviceRegistry(logger, aggregatedMetricsBufferSize)
	if err != nil {
		return nil, err
	}
	s.metrics, err = newMetricsRegistry(logger)
	return s, err
}

func (s *sysman) PollAggregatedMetrics() {
	s.devices.pollAggregatedMetrics()
}

func (s *sysman) Scrape() pmetric.Metrics {
	return s.metrics.scrape(s.devices)
}
