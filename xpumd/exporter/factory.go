//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package exporter

import (
	"context"
	"fmt"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/exporter"
	"go.opentelemetry.io/collector/exporter/exporterhelper"

	"github.com/intel/xpumanager/exporter/internal/metadata"
)

// NewFactory creates a factory for the exporter.
func NewFactory() exporter.Factory {
	return exporter.NewFactory(
		metadata.Type,
		defaultConfig,
		exporter.WithMetrics(createMetricsExporter, metadata.MetricsStability),
	)
}

func createMetricsExporter(ctx context.Context, settings exporter.Settings, ecfg component.Config) (exporter.Metrics, error) {
	cfg, ok := ecfg.(*Config)
	if !ok {
		return nil, fmt.Errorf("invalid config type: %T", ecfg)
	}

	e := newXpuInfoExporter(cfg, settings)

	return exporterhelper.NewMetrics(
		ctx,
		settings,
		ecfg,
		e.pushMetrics,
		exporterhelper.WithStart(e.start),
		exporterhelper.WithShutdown(e.shutdown),
	)
}
