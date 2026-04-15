//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package exporter

import (
	"context"
	"fmt"
	"sync"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/exporter"
	"go.opentelemetry.io/collector/exporter/exporterhelper"

	"github.com/intel/xpumanager/xpumd/exporter/internal/metadata"
)

// NewFactory creates a factory for the exporter.
func NewFactory() exporter.Factory {
	p := &exporterProvider{}
	return exporter.NewFactory(
		metadata.Type,
		defaultConfig,
		exporter.WithMetrics(p.createMetricsExporter, metadata.MetricsStability),
		exporter.WithLogs(p.createLogsExporter, metadata.LogsStability),
	)
}

// exporterProvider holds the shared xpuInfoExporter instance so that the
// metrics and logs pipelines operate on the same gRPC server.
type exporterProvider struct {
	once sync.Once
	e    *xpuInfoExporter
}

func (p *exporterProvider) get(cfg *Config, settings exporter.Settings) *xpuInfoExporter {
	p.once.Do(func() {
		p.e = newXpuInfoExporter(cfg, settings)
	})
	return p.e
}

func (p *exporterProvider) createMetricsExporter(ctx context.Context, settings exporter.Settings, ecfg component.Config) (exporter.Metrics, error) {
	cfg, ok := ecfg.(*Config)
	if !ok {
		return nil, fmt.Errorf("invalid config type: %T", ecfg)
	}

	e := p.get(cfg, settings)

	return exporterhelper.NewMetrics(
		ctx,
		settings,
		ecfg,
		e.pushMetrics,
		exporterhelper.WithStart(e.start),
		exporterhelper.WithShutdown(e.shutdown),
	)
}

func (p *exporterProvider) createLogsExporter(ctx context.Context, settings exporter.Settings, ecfg component.Config) (exporter.Logs, error) {
	cfg, ok := ecfg.(*Config)
	if !ok {
		return nil, fmt.Errorf("invalid config type: %T", ecfg)
	}

	e := p.get(cfg, settings)

	return exporterhelper.NewLogs(
		ctx,
		settings,
		ecfg,
		e.pushLogs,
		exporterhelper.WithStart(e.start),
		exporterhelper.WithShutdown(e.shutdown),
	)
}
