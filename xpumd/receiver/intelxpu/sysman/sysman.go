//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"fmt"
	"runtime/debug"
	"sync"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.uber.org/zap"

	l0sysman "github.com/intel/level-zero-go/sysman"
	"github.com/intel/xpumanager/xpumd/receiver/intelxpu/sysman/internal/metadata"
)

const (
	// Maximum number of independent metric readers for aggregated metrics.
	maxAggregatedMetricsReaders = 1
)

// isNeededFunc tells whether any of the metrics needing a driver query is enabled in the config.
type isNeededFunc func(*metadata.MetricsConfig) bool

// instanceHook is a scrape or sampling hook of the subsystem instances.
// A hook function returning false is not called again (for that instance), e.g. after a failed query.
type instanceHook[T any] struct {
	isNeeded   isNeededFunc
	isSampling bool
	// init is called on setup, the hook is not added for an instance whose init fails
	init  func(T) bool
	addTo func(*device, T)
}

// onScrape specifies a scrape hook, added to the device when needed.
func onScrape[T any](scrape func(T, *metadata.MetricsBuilder, pcommon.Timestamp) bool, isNeeded isNeededFunc) instanceHook[T] {
	return instanceHook[T]{isNeeded: isNeeded, addTo: func(d *device, inst T) {
		stopped := false
		d.scrapeFuncs = append(d.scrapeFuncs, func(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
			if !stopped {
				stopped = !scrape(inst, mb, ts)
			}
		})
	}}
}

// onSampling specifies a sampling hook called by the sampler, added to the device when needed.
func onSampling[T any](sampleFn func(T) bool, isNeeded isNeededFunc) instanceHook[T] {
	return instanceHook[T]{isNeeded: isNeeded, isSampling: true, addTo: func(d *device, inst T) {
		stopped := false
		d.samplingFuncs = append(d.samplingFuncs, func() {
			if !stopped {
				stopped = !sampleFn(inst)
			}
		})
	}}
}

// withInit adds an init function, e.g. for checking that the metrics are available,
// or for reading the initial counter values.
func (hook instanceHook[T]) withInit(init func(T) bool) instanceHook[T] {
	hook.init = init
	return hook
}

type subsystem struct {
	// setup enumerates the device instances and adds their needed hooks to the device
	setup func(*device)
	// isSamplingNeeded tells whether any sampling function is needed, for starting the sampler only when required
	isSamplingNeeded isNeededFunc
	// isNeeded tells whether any function is needed, only needed for testing
	isNeeded isNeededFunc
}

var subsystems = map[string]subsystem{}

// registerSubsystem registers a subsystem with its instance enumeration and functions.
func registerSubsystem[T any](name string, enum func(*device) []T, hooks ...instanceHook[T]) {
	if _, exists := subsystems[name]; exists {
		panic("subsystem already registered: " + name)
	}
	subsystems[name] = newSubsystem(name, enum, hooks...)
}

func newSubsystem[T any](name string, enum func(*device) []T, hooks ...instanceHook[T]) subsystem {
	s := subsystem{}
	s.setup = func(d *device) {
		neededHooks := []instanceHook[T]{}
		for _, hook := range hooks {
			if hook.isNeeded(d.metrics) {
				neededHooks = append(neededHooks, hook)
			}
		}
		if len(neededHooks) == 0 {
			d.logger.Debugw("Sysman (metric type) subsystem skipped, all of its metrics are disabled", "subsystem", name, "device", d.attributes.HwName)
			return
		}
		unavailable := 0
		for _, inst := range enum(d) {
			added := false
			for _, hook := range neededHooks {
				if hook.init != nil && !hook.init(inst) {
					continue
				}
				hook.addTo(d, inst)
				added = true
			}
			if !added {
				unavailable++
			}
		}
		if unavailable > 0 {
			d.logger.Infow("some Sysman metric type subset(s) skipped as no (enabled) metrics are available for them",
				"subsystem", name, "count", unavailable, "device", d.attributes.HwName)
		}
	}
	s.isSamplingNeeded = func(c *metadata.MetricsConfig) bool {
		for _, hook := range hooks {
			if hook.isSampling && hook.isNeeded(c) {
				return true
			}
		}
		return false
	}
	s.isNeeded = func(c *metadata.MetricsConfig) bool {
		for _, hook := range hooks {
			if hook.isNeeded(c) {
				return true
			}
		}
		return false
	}
	return s
}

// needSampling tells whether the enabled subsystems and metrics need the sampler.
func needSampling(cfg *Config) bool {
	for name, s := range subsystems {
		if cfg.subsystemEnabled(name) && s.isSamplingNeeded(&cfg.Metrics) {
			return true
		}
	}
	return false
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
		if bi, ok := debug.ReadBuildInfo(); ok {
			logger.Infow("build info", "settings", bi.Settings)
		}
		logger.Debug("Sysman init...")
		if err := l0sysman.Init(0); err != nil {
			p.err = fmt.Errorf("failed to initialize L0 Sysman API (likely a Level-Zero driver / device access issue): %w", err)
			return
		}
		logger.Debug("Sysman drivers/devices/metrics enumeration starting")
		p.devices, p.err = newDeviceRegistry(logger, cfg)
		if p.err != nil {
			p.err = fmt.Errorf("failed to initialize device registry: %w", p.err)
		}
		logger.Debug("Sysman drivers/devices/metrics enumeration completed")
	})
	return p.devices, p.err
}
