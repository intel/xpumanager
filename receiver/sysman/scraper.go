//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/intel/xpumanager/receiver/sysman/internal/metadata"
	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.opentelemetry.io/collector/scraper"
	"go.uber.org/zap"

	l0sysman "github.com/intel/level-zero-go/sysman"
)

type sysmanScraper struct {
	settings scraper.Settings
	cfg      *Config
	wg       sync.WaitGroup
	mb       *metadata.MetricsBuilder
	devices  *deviceRegistry
	logger   *zap.SugaredLogger
	stop     context.CancelFunc
}

func newSysmanScraper(_ context.Context, settings scraper.Settings, cfg *Config) (*sysmanScraper, error) {
	logger := settings.Logger.WithOptions(zap.IncreaseLevel(cfg.LogLevel)).Sugar()

	// Warn about potentially config issues.
	// Cannot do in Config.Validate() as logger is not available there.
	if cfg.SamplingInterval < 100*time.Millisecond {
		logger.Warnw("short sampling_interval may cause high CPU usage",
			"samplingInterval", cfg.SamplingInterval,
		)
	}

	// Initialize Sysman
	err := l0sysman.Init(0)
	if err != nil {
		return nil, fmt.Errorf("failed to initialize L0 Sysman API (likely a Level-Zero driver / device access issue): %w", err)
	}

	devices, err := newDeviceRegistry(logger, cfg.aggregatedMetricsBufferSize)
	if err != nil {
		return nil, fmt.Errorf("failed to initialize device registry: %w", err)
	}

	return &sysmanScraper{
		settings: settings,
		cfg:      cfg,
		mb:       metadata.NewMetricsBuilder(cfg.MetricsBuilderConfig, settings),
		devices:  devices,
		logger:   logger,
	}, nil
}

func (s *sysmanScraper) start(ctx context.Context, _ component.Host) error {
	ctx, s.stop = context.WithCancel(ctx)

	s.wg.Go(func() { s.runSampler(ctx) })

	return nil
}

func (s *sysmanScraper) shutdown(ctx context.Context) error {
	if s.stop != nil {
		s.stop()
	}
	s.wg.Wait()
	return s.logger.Sync()
}

// runSampler samples the aggregated device metrics.
func (s *sysmanScraper) runSampler(ctx context.Context) {
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

func (s *sysmanScraper) scrape(ctx context.Context) (pmetric.Metrics, error) {
	now := pcommon.NewTimestampFromTime(time.Now())

	s.devices.scrape(s.mb, now)

	return s.mb.Emit(), nil
}
