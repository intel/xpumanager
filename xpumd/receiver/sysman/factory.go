//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"context"
	"fmt"

	"github.com/intel/xpumanager/xpumd/receiver/sysman/internal/metadata"
	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/scraper"
)

// NewFactory for Sysman scraper.
func NewFactory() scraper.Factory {
	return scraper.NewFactory(metadata.Type, defaultConfig, scraper.WithMetrics(createMetricsScraper, metadata.MetricsStability))
}

func createMetricsScraper(
	ctx context.Context,
	settings scraper.Settings,
	scfg component.Config,
) (scraper.Metrics, error) {
	cfg, ok := scfg.(*Config)
	if !ok {
		return nil, fmt.Errorf("invalid config type: %T", scfg)
	}

	s, err := newSysmanScraper(ctx, settings, cfg)
	if err != nil {
		return nil, err
	}

	return scraper.NewMetrics(
		s.scrape,
		scraper.WithStart(s.start),
		scraper.WithShutdown(s.shutdown),
	)
}
