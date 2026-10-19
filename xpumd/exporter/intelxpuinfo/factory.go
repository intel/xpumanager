//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpuinfo

import (
	"context"
	"fmt"
	"sync"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/exporter"
	"go.opentelemetry.io/collector/exporter/exporterhelper"

	"github.com/intel/xpumanager/xpumd/exporter/intelxpuinfo/internal/metadata"
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

// exporterProvider holds the shared xpuInfoExporter instances so that the
// metrics and logs pipelines of each component ID operate on the same gRPC server.
type exporterProvider struct {
	mu        sync.Mutex
	exporters map[component.ID]*sharedExporter
}

// sharedExporter is an xpuInfoExporter shared by all pipelines for one component ID.
type sharedExporter struct {
	exporter *xpuInfoExporter
	provider *exporterProvider
	id       component.ID
	refs     int // counts pipelines that haven't been shut down, protected by p.mu
}

// release drops one ref, shutting down when all refs are gone.
func (se *sharedExporter) release(ctx context.Context) error {
	se.provider.mu.Lock()
	defer se.provider.mu.Unlock()

	se.refs--
	if se.refs > 0 {
		return nil
	}
	delete(se.provider.exporters, se.id)
	return se.exporter.shutdown(ctx)
}

// exporterRef is one pipeline's reference to a sharedExporter.
type exporterRef struct {
	*sharedExporter
	once sync.Once
}

// shutdown releases the reference.
func (r *exporterRef) shutdown(ctx context.Context) error {
	var err error
	r.once.Do(func() {
		err = r.release(ctx)
	})
	return err
}

// acquire returns a new reference to a shared exporter.
func (p *exporterProvider) acquire(cfg *Config, settings exporter.Settings) *exporterRef {
	p.mu.Lock()
	defer p.mu.Unlock()

	if p.exporters == nil {
		p.exporters = make(map[component.ID]*sharedExporter)
	}
	se, ok := p.exporters[settings.ID]
	if !ok {
		se = &sharedExporter{exporter: newXpuInfoExporter(cfg, settings), provider: p, id: settings.ID}
		p.exporters[settings.ID] = se
	}
	se.refs++
	return &exporterRef{sharedExporter: se}
}

func (p *exporterProvider) createMetricsExporter(ctx context.Context, settings exporter.Settings, ecfg component.Config) (exporter.Metrics, error) {
	cfg, ok := ecfg.(*Config)
	if !ok {
		return nil, fmt.Errorf("invalid config type: %T", ecfg)
	}

	ref := p.acquire(cfg, settings)

	exp, err := exporterhelper.NewMetrics(
		ctx,
		settings,
		ecfg,
		ref.exporter.pushMetrics,
		exporterhelper.WithStart(ref.exporter.start),
		exporterhelper.WithShutdown(ref.shutdown),
	)
	if err != nil {
		_ = ref.shutdown(ctx)
		return nil, err
	}
	return exp, nil
}

func (p *exporterProvider) createLogsExporter(ctx context.Context, settings exporter.Settings, ecfg component.Config) (exporter.Logs, error) {
	cfg, ok := ecfg.(*Config)
	if !ok {
		return nil, fmt.Errorf("invalid config type: %T", ecfg)
	}

	ref := p.acquire(cfg, settings)

	exp, err := exporterhelper.NewLogs(
		ctx,
		settings,
		ecfg,
		ref.exporter.pushLogs,
		exporterhelper.WithStart(ref.exporter.start),
		exporterhelper.WithShutdown(ref.shutdown),
	)
	if err != nil {
		_ = ref.shutdown(ctx)
		return nil, err
	}
	return exp, nil
}
