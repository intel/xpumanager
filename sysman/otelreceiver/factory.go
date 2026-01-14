//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package otelreceiver

import (
	"context"
	"fmt"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/receiver"
	"go.opentelemetry.io/collector/scraper"
	"go.opentelemetry.io/collector/scraper/scraperhelper"
)

var componentType = component.MustNewType("sysman")

// NewFactory creates a factory for the sysman receiver
func NewFactory() receiver.Factory {
	const stability = component.StabilityLevelDevelopment
	return receiver.NewFactory(
		componentType,
		defaultConfig,
		receiver.WithMetrics(createReceiver, stability),
	)
}

func createReceiver(_ context.Context, settings receiver.Settings, cfg component.Config, consumer consumer.Metrics) (receiver.Metrics, error) {
	rcfg, ok := cfg.(*Config)
	if !ok {
		return nil, fmt.Errorf("invalid config type: %T", cfg)
	}

	sr, err := newSysmanReceiver(settings, rcfg)
	if err != nil {
		return nil, err
	}

	s, err := scraper.NewMetrics(
		sr.scrape,
		scraper.WithStart(sr.Start),
		scraper.WithShutdown(sr.Shutdown),
	)
	if err != nil {
		return nil, err
	}

	return scraperhelper.NewMetricsController(
		&rcfg.ControllerConfig,
		settings,
		consumer,
		scraperhelper.AddScraper(componentType, s),
	)
}
