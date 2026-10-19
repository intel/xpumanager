//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpuinfo

import (
	"context"
	"net"
	"path/filepath"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/component/componenttest"
	"go.opentelemetry.io/collector/exporter"
	"go.opentelemetry.io/collector/exporter/exportertest"
)

func TestCreateExporterInvalidConfigType(t *testing.T) {
	p := &exporterProvider{}

	_, err := p.createMetricsExporter(context.Background(), exportertest.NewNopSettings(typ), &struct{}{})
	assert.ErrorContains(t, err, "invalid config type")

	_, err = p.createLogsExporter(context.Background(), exportertest.NewNopSettings(typ), &struct{}{})
	assert.ErrorContains(t, err, "invalid config type")
}

func TestExporterProvider(t *testing.T) {
	ctx := context.Background()
	host := componenttest.NewNopHost()
	p := &exporterProvider{}

	type pipelines struct {
		endpoint string
		metrics  exporter.Metrics
		logs     exporter.Logs
	}
	createPipelines := func(name string) pipelines {
		cfg, ok := defaultConfig().(*Config)
		require.True(t, ok)
		cfg.NetAddr.Endpoint = filepath.Join(t.TempDir(), name+".sock")
		settings := exportertest.NewNopSettings(typ)
		settings.ID = component.NewIDWithName(typ, name)

		m, err := p.createMetricsExporter(ctx, settings, cfg)
		require.NoError(t, err)
		l, err := p.createLogsExporter(ctx, settings, cfg)
		require.NoError(t, err)
		require.NoError(t, m.Start(ctx, host))
		require.NoError(t, l.Start(ctx, host))
		return pipelines{endpoint: cfg.NetAddr.Endpoint, metrics: m, logs: l}
	}
	isListening := func(endpoint string) bool {
		conn, err := net.Dial("unix", endpoint)
		if err != nil {
			return false
		}
		_ = conn.Close()
		return true
	}

	foo := createPipelines("foo")
	bar := createPipelines("bar")
	assert.True(t, isListening(foo.endpoint))
	assert.True(t, isListening(bar.endpoint))

	// Repeated shutdown of one pipeline must drop only one reference.
	require.NoError(t, foo.metrics.Shutdown(ctx))
	require.NoError(t, foo.metrics.Shutdown(ctx))
	assert.True(t, isListening(foo.endpoint))

	require.NoError(t, foo.logs.Shutdown(ctx))
	assert.False(t, isListening(foo.endpoint))
	assert.True(t, isListening(bar.endpoint))

	require.NoError(t, bar.metrics.Shutdown(ctx))
	require.NoError(t, bar.logs.Shutdown(ctx))
	assert.False(t, isListening(bar.endpoint))
}
