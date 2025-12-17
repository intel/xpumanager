//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package otelreceiver

import (
	"context"
	"fmt"
	"sync"
	"time"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/receiver"
	"go.uber.org/zap"

	"github.com/intel/xpu-exporter/sysman"
)

func newSysmanReceiver(settings receiver.Settings, cfg *Config, consumer consumer.Metrics) (receiver.Metrics, error) {
	logger := settings.Logger.WithOptions(zap.IncreaseLevel(cfg.LogLevel)).Sugar()

	// Warn about potentially config issues.
	// Cannot do in Config.Validate() as logger is not available there.
	if cfg.CollectInterval < 2*cfg.SampleInterval {
		orig := cfg.SampleInterval
		cfg.SampleInterval = cfg.CollectInterval / 2
		logger.Warnw("collectInterval must be >= 2 * sampleInterval to guarantee data integrity, sampleInterval adjusted",
			"collectInterval", cfg.CollectInterval,
			"sampleInterval", cfg.SampleInterval,
			"oldSampleInterval", orig,
		)
	}
	if cfg.SampleInterval < 100*time.Millisecond {
		logger.Warnw("short sampleInterval may cause high CPU usage",
			"sampleInterval", cfg.SampleInterval,
		)
	}

	// Initialize Sysman
	// Reserve one second (or at least 10 samples) extra for the aggregated sample buffer to mitigate possible jitter to not lose samples
	const sampleBufferMinExtraSamples = 10
	aggregatedSampleCount := cfg.CollectInterval/cfg.SampleInterval + max(time.Second/cfg.SampleInterval, sampleBufferMinExtraSamples)

	s, err := sysman.New(int(aggregatedSampleCount))
	if err != nil {
		return nil, fmt.Errorf("failed to initialize L0 Sysman API (likely a Level-Zero driver / device access issue): %w", err)
	}

	return &sysmanReceiver{
		cfg:      cfg,
		settings: settings,
		logger:   logger,
		consumer: consumer,
		sysman:   s,
	}, nil
}

// sysmanReceiver is a no-op receiver implementation
type sysmanReceiver struct {
	cfg      *Config
	wg       sync.WaitGroup
	settings receiver.Settings
	consumer consumer.Metrics
	logger   *zap.SugaredLogger
	stop     context.CancelFunc
	sysman   sysman.Handle
}

// Start starts the receiver.
func (r *sysmanReceiver) Start(ctx context.Context, host component.Host) error {
	ctx, r.stop = context.WithCancel(ctx)

	r.wg.Go(func() { r.runSampler(ctx) })
	r.wg.Go(func() { r.runScraper(ctx) })

	return nil
}

// Shutdown stops the receiver.
func (r *sysmanReceiver) Shutdown(ctx context.Context) error {
	if r.stop != nil {
		r.stop()
	}
	r.wg.Wait()
	r.logger.Sync()
	return nil
}

// runSampler samples the aggregated device metrics.
func (r *sysmanReceiver) runSampler(ctx context.Context) {
	ticker := time.NewTicker(r.cfg.SampleInterval)
	defer ticker.Stop()
	for {
		select {
		case <-ticker.C:
			r.sysman.PollAggregatedMetrics()
		case <-ctx.Done():
			return
		}
	}
}

// runScraper scrapes the device metrics and sends them to the consumer.
func (r *sysmanReceiver) runScraper(ctx context.Context) {
	ticker := time.NewTicker(r.cfg.CollectInterval)
	defer ticker.Stop()
	for {
		select {
		case <-ticker.C:
			r.logger.Info("collecting device metrics")
		case <-ctx.Done():
			return
		}
	}
}
