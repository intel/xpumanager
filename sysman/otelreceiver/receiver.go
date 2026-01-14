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
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.opentelemetry.io/collector/receiver"
	"go.uber.org/zap"

	"github.com/intel/xpumanager/sysman"
)

func newSysmanReceiver(settings receiver.Settings, cfg *Config) (*sysmanReceiver, error) {
	logger := settings.Logger.WithOptions(zap.IncreaseLevel(cfg.LogLevel)).Sugar()

	// Warn about potentially config issues.
	// Cannot do in Config.Validate() as logger is not available there.
	if cfg.CollectionInterval < 2*cfg.SamplingInterval {
		orig := cfg.SamplingInterval
		cfg.SamplingInterval = cfg.CollectionInterval / 2
		logger.Warnw("collectionInterval must be >= 2 * samplingInterval to guarantee data integrity, samplingInterval adjusted",
			"collectionInterval", cfg.CollectionInterval,
			"samplingInterval", cfg.SamplingInterval,
			"oldSamplingInterval", orig,
		)
	}
	if cfg.SamplingInterval < 100*time.Millisecond {
		logger.Warnw("short samplingInterval may cause high CPU usage",
			"samplingInterval", cfg.SamplingInterval,
		)
	}

	// Initialize Sysman
	// Reserve one second (or at least 10 samples) extra for the aggregated sample buffer to mitigate possible jitter to not lose samples
	const sampleBufferMinExtraSamples = 10
	aggregatedSampleCount := cfg.CollectionInterval/cfg.SamplingInterval + max(time.Second/cfg.SamplingInterval, sampleBufferMinExtraSamples)

	s, err := sysman.New(logger, int(aggregatedSampleCount))
	if err != nil {
		return nil, fmt.Errorf("failed to initialize L0 Sysman API (likely a Level-Zero driver / device access issue): %w", err)
	}

	return &sysmanReceiver{
		cfg:      cfg,
		settings: settings,
		logger:   logger,
		sysman:   s,
	}, nil
}

// sysmanReceiver is a no-op receiver implementation
type sysmanReceiver struct {
	cfg      *Config
	wg       sync.WaitGroup
	settings receiver.Settings
	logger   *zap.SugaredLogger
	stop     context.CancelFunc
	sysman   sysman.Handle
}

// Start starts the receiver.
func (r *sysmanReceiver) Start(ctx context.Context, host component.Host) error {
	ctx, r.stop = context.WithCancel(ctx)

	r.wg.Go(func() { r.runSampler(ctx) })

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
	ticker := time.NewTicker(r.cfg.SamplingInterval)
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

// scrape scrapes the device metrics and sends them to the consumer.
func (r *sysmanReceiver) scrape(_ context.Context) (pmetric.Metrics, error) {
	return r.sysman.Scrape(), nil
}
