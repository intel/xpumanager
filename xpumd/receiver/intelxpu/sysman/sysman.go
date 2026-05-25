//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"fmt"
	"sync"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.uber.org/zap"

	l0sysman "github.com/intel/level-zero-go/sysman"
	"github.com/intel/xpumanager/xpumd/receiver/sysman/internal/metadata"
)

const (
	// Maximum number of independent metric readers for aggregated metrics.
	maxAggregatedMetricsReaders = 1
)

type instanceScraper interface {
	scrape(*metadata.MetricsBuilder, pcommon.Timestamp)
	pollAggregatedMetrics()
}

type enumDeviceFunc func(*device) []instanceScraper

type subsystem struct {
	enumDevice enumDeviceFunc
}

var subsystems = map[string]subsystem{}

func registerSubsystem(name string, enumDevice enumDeviceFunc) {
	if _, exists := subsystems[name]; exists {
		panic("subsystem already registered: " + name)
	}
	subsystems[name] = subsystem{
		enumDevice: enumDevice,
	}
}

// sysmanProvider initialises the L0 Sysman API and the device registry exactly
// once, regardless of how many scrapers are created from the same factory.
type sysmanProvider struct {
	once    sync.Once
	devices *deviceRegistry
	err     error
}

// get returns the shared device registry, initialising it on the first call.
// logger and cfg are only used on that first call; subsequent callers
// receive the cached result.
func (p *sysmanProvider) get(logger *zap.SugaredLogger, cfg *Config) (*deviceRegistry, error) {
	p.once.Do(func() {
		if err := l0sysman.Init(0); err != nil {
			p.err = fmt.Errorf("failed to initialize L0 Sysman API (likely a Level-Zero driver / device access issue): %w", err)
			return
		}
		p.devices, p.err = newDeviceRegistry(logger, cfg.aggregatedMetricsBufferSize)
		if p.err != nil {
			p.err = fmt.Errorf("failed to initialize device registry: %w", p.err)
		}
	})
	return p.devices, p.err
}
