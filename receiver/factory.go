//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package receiver

import (
	"context"
	"fmt"
	"time"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/receiver"
	"go.opentelemetry.io/collector/scraper/scraperhelper"

	"github.com/intel/xpumanager/receiver/internal/metadata"
	"github.com/intel/xpumanager/receiver/sysman"
)

var (
	sysmanFactory = sysman.NewFactory()
)

// NewFactory creates a factory for the receiver.
func NewFactory() receiver.Factory {
	return receiver.NewFactory(
		metadata.Type,
		defaultConfig,
		receiver.WithMetrics(createReceiver, metadata.MetricsStability),
	)
}

func createReceiver(_ context.Context, settings receiver.Settings, rcfg component.Config, consumer consumer.Metrics) (receiver.Metrics, error) {
	cfg, ok := rcfg.(*Config)
	if !ok {
		return nil, fmt.Errorf("invalid config type: %T", rcfg)
	}

	cfg.lint(settings.Logger.Sugar())

	// Reserve one second (or at least 10 samples) extra for the aggregated sample buffer to mitigate possible jitter to not lose samples
	const sampleBufferMinExtraSamples = 10
	aggregatedSampleCount := cfg.CollectionInterval/cfg.SamplingInterval + max(time.Second/cfg.SamplingInterval, sampleBufferMinExtraSamples)
	cfg.SetAggregatedMetricsBufferSize(int(aggregatedSampleCount))

	return scraperhelper.NewMetricsController(
		&cfg.ControllerConfig,
		settings,
		consumer,
		scraperhelper.AddFactoryWithConfig(sysmanFactory, cfg.Config),
	)
}
