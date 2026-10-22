//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"context"
	"fmt"
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
	settings  scraper.Settings
	cfg       *Config
	wg        sync.WaitGroup
	mb        *metadata.MetricsBuilder
	telemetry *metadata.TelemetryBuilder
	devices   *deviceRegistry
	logger    *zap.SugaredLogger
	stop      context.CancelFunc
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

	telemetry, err := metadata.NewTelemetryBuilder(settings.TelemetrySettings)
	if err != nil {
		return nil, fmt.Errorf("creating telemetry builder: %w", err)
	}

	return &sysmanMetricsScraper{
		settings:  settings,
		cfg:       cfg,
		mb:        metadata.NewMetricsBuilder(cfg.MetricsBuilderConfig, settings),
		telemetry: telemetry,
		devices:   devices,
		logger:    logger,
	}, nil
}

func (s *sysmanMetricsScraper) start(_ context.Context, _ component.Host) error {
	// NOTE: the context passed to start() is only meant for the startup itself
	ctx, stop := context.WithCancel(context.Background())
	s.stop = stop

	s.wg.Go(func() { s.runSampler(ctx) })

	return nil
}

func (s *sysmanMetricsScraper) shutdown(ctx context.Context) error {
	if s.stop != nil {
		s.stop()
	}
	s.wg.Wait()
	s.telemetry.Shutdown()
	return s.logger.Sync()
}

// runSampler samples the aggregated device metrics.
func (s *sysmanMetricsScraper) runSampler(ctx context.Context) {
	ticker := time.NewTicker(s.cfg.SamplingInterval)
	defer ticker.Stop()
	last := time.Now()
	for {
		select {
		case t := <-ticker.C:
			si := s.cfg.SamplingInterval
			if missed := int64((t.Sub(last)+si/2)/si) - 1; missed > 0 {
				s.telemetry.SysmanSamplerMissedDeadlines.Add(ctx, missed)
			}
			last = t

			s.devices.pollAggregatedMetrics()
			s.telemetry.SysmanSamplerIntervalUsage.Record(ctx, float64(time.Since(t))/float64(si))
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
