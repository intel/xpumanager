//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package exporter

import (
	"context"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/exporter"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.uber.org/zap"

	"github.com/intel/xpumanager/exporter/internal/metadata"
)

type xpuInfoExporter struct {
	cfg       *Config
	logger    *zap.SugaredLogger
	telemetry *metadata.TelemetryBuilder
	settings  exporter.Settings
}

func newXpuInfoExporter(cfg *Config, settings exporter.Settings) *xpuInfoExporter {
	return &xpuInfoExporter{
		cfg:      cfg,
		logger:   settings.Logger.Sugar(),
		settings: settings,
	}
}

func (e *xpuInfoExporter) start(ctx context.Context, host component.Host) error {
	e.logger.Info("Starting XPU Info exporter")

	tb, err := metadata.NewTelemetryBuilder(e.settings.TelemetrySettings)
	if err != nil {
		return err
	}
	e.telemetry = tb

	return nil
}

func (e *xpuInfoExporter) shutdown(ctx context.Context) error {
	e.logger.Info("Shutting down XPU Info exporter")

	return nil
}

func (e *xpuInfoExporter) pushMetrics(ctx context.Context, md pmetric.Metrics) error {
	e.logger.Infow("Pushing metrics", "dataPoints", md.DataPointCount())
	return nil
}
