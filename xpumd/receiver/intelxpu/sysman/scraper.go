//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"context"
	"sync"
	"time"

	"github.com/intel/xpumanager/xpumd/receiver/intelxpu/sysman/internal/metadata"
	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.opentelemetry.io/collector/scraper"
	"go.uber.org/zap"
)

type sysmanMetricsScraper struct {
	settings scraper.Settings
	cfg      *Config
	wg       sync.WaitGroup
	mb       *metadata.MetricsBuilder
	devices  *deviceRegistry
	logger   *zap.SugaredLogger
	stop     context.CancelFunc
}

func newSysmanMetricsScraper(_ context.Context, settings scraper.Settings, cfg *Config, devices *deviceRegistry) (*sysmanMetricsScraper, error) {
	logger := settings.Logger.Sugar()

	// Warn about potentially config issues.
	// Cannot do in Config.Validate() as logger is not available there.
	if cfg.SamplingInterval < 100*time.Millisecond {
		logger.Warnw("short sampling_interval may cause high CPU usage",
			"samplingInterval", cfg.SamplingInterval,
		)
	}

	return &sysmanMetricsScraper{
		settings: settings,
		cfg:      cfg,
		mb:       metadata.NewMetricsBuilder(cfg.MetricsBuilderConfig, settings),
		devices:  devices,
		logger:   logger,
	}, nil
}

func (s *sysmanMetricsScraper) start(ctx context.Context, _ component.Host) error {
	ctx, s.stop = context.WithCancel(ctx)

	s.wg.Go(func() { s.runSampler(ctx) })

	return nil
}

func (s *sysmanMetricsScraper) shutdown(ctx context.Context) error {
	if s.stop != nil {
		s.stop()
	}
	s.wg.Wait()
	return s.logger.Sync()
}

// runSampler samples the aggregated device metrics.
func (s *sysmanMetricsScraper) runSampler(ctx context.Context) {
	ticker := time.NewTicker(s.cfg.SamplingInterval)
	defer ticker.Stop()
	for {
		select {
		case <-ticker.C:
			s.devices.pollAggregatedMetrics()
		case <-ctx.Done():
			return
		}
	}
}

func (s *sysmanMetricsScraper) scrape(ctx context.Context) (pmetric.Metrics, error) {
	now := pcommon.NewTimestampFromTime(time.Now())

	s.devices.scrape(s.mb, now)

	return s.mb.Emit(), nil
}
