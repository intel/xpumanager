//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"context"
	"fmt"

	"github.com/intel/xpumanager/xpumd/receiver/intelxpu/sysman/internal/metadata"
	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/scraper"
	"go.uber.org/zap"
)

// Handle is a wrapper that provides access to both periodic scraper (metrics)
// and async events.
type Handle struct {
	scraper.Factory
	provider *sysmanProvider
}

// New returns the sysman Factory.
func New() *Handle {
	p := &sysmanProvider{}
	return &Handle{
		Factory: scraper.NewFactory(
			metadata.Type,
			defaultConfig,
			scraper.WithMetrics(makeMetricsScraper(p), metadata.MetricsStability),
		),
		provider: p,
	}
}

// NewFactory is a convenience function required by generated mdatagen tests.
func NewFactory() scraper.Factory {
	return New()
}

func makeMetricsScraper(p *sysmanProvider) scraper.CreateMetricsFunc {
	return func(ctx context.Context, settings scraper.Settings, scfg component.Config) (scraper.Metrics, error) {
		cfg, ok := scfg.(*Config)
		if !ok {
			return nil, fmt.Errorf("invalid config type: %T", scfg)
		}

		logger := settings.Logger.WithOptions(zap.IncreaseLevel(cfg.LogLevel)).Sugar()
		devices, err := p.get(logger, cfg)
		if err != nil {
			return nil, err
		}

		s, err := newSysmanMetricsScraper(ctx, settings, cfg, devices)
		if err != nil {
			return nil, err
		}

		return scraper.NewMetrics(
			s.scrape,
			scraper.WithStart(s.start),
			scraper.WithShutdown(s.shutdown),
		)
	}
}

// CreateLogsReceiver creates a sysman logs (events) receiver.
func (f *Handle) CreateLogsReceiver(settings component.TelemetrySettings, cfg *Config, nextConsumer consumer.Logs) (component.Component, error) {
	logger := settings.Logger.WithOptions(zap.IncreaseLevel(cfg.LogLevel)).Sugar()
	devices, err := f.provider.get(logger, cfg)
	if err != nil {
		return nil, err
	}
	return newSysmanEventsReceiver(devices, logger, nextConsumer)
}
