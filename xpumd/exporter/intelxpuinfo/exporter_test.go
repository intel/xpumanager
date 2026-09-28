//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpuinfo

import (
	"context"
	"net"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.opentelemetry.io/collector/component/componenttest"
	"go.opentelemetry.io/collector/config/confignet"
	"go.opentelemetry.io/collector/exporter/exportertest"
	"go.uber.org/zap"
	"go.uber.org/zap/zaptest/observer"
)

// TestStartEndpointSetup tests the unix socket endpoint handling.
func TestStartEndpointSetup(t *testing.T) {
	tests := []struct {
		name      string
		setup     func(t *testing.T, netAddr *confignet.AddrConfig)
		expectErr string
	}{
		{
			name: "endpoint derived from XDG_RUNTIME_DIR",
			setup: func(t *testing.T, _ *confignet.AddrConfig) {
				t.Setenv("XDG_RUNTIME_DIR", t.TempDir())
			},
		},
		{
			name: "no endpoint in config, no XDG_RUNTIME_DIR",
			setup: func(t *testing.T, _ *confignet.AddrConfig) {
				t.Setenv("XDG_RUNTIME_DIR", "")
			},
			expectErr: "neither XDG_RUNTIME_DIR set",
		},
		{
			name: "socket directory is created on demand",
			setup: func(t *testing.T, netAddr *confignet.AddrConfig) {
				netAddr.Endpoint = filepath.Join(t.TempDir(), "sub", "xpuinfo.sock")
			},
		},
		{
			name: "socket directory is a regular file",
			setup: func(t *testing.T, netAddr *confignet.AddrConfig) {
				dir := t.TempDir()
				blocker := filepath.Join(dir, "blocker")
				require.NoError(t, os.WriteFile(blocker, nil, 0o600))
				netAddr.Endpoint = filepath.Join(blocker, "xpuinfo.sock")
			},
			expectErr: "not a directory",
		},
		{
			name: "stat on socket directory fails",
			setup: func(t *testing.T, netAddr *confignet.AddrConfig) {
				dir := t.TempDir()
				blocker := filepath.Join(dir, "blocker")
				require.NoError(t, os.WriteFile(blocker, nil, 0o600))
				// The parent of the dir is a regular file, so the stat fails with something other than ENOENT
				netAddr.Endpoint = filepath.Join(blocker, "sub", "xpuinfo.sock")
			},
			expectErr: "failed to stat unix socket directory",
		},
		{
			name: "stale endpoint file is not a socket",
			setup: func(t *testing.T, netAddr *confignet.AddrConfig) {
				endpoint := filepath.Join(t.TempDir(), "xpuinfo.sock")
				require.NoError(t, os.WriteFile(endpoint, nil, 0o600))
				netAddr.Endpoint = endpoint
			},
			expectErr: "it's not a socket",
		},
		{
			name: "leftover socket is removed",
			setup: func(t *testing.T, netAddr *confignet.AddrConfig) {
				endpoint := filepath.Join(t.TempDir(), "xpuinfo.sock")
				// Keep the listener open so that the socket file stays in place. Go unlinks it when the listener is closed.
				lis, err := net.Listen("unix", endpoint)
				require.NoError(t, err)
				t.Cleanup(func() { _ = lis.Close() })
				netAddr.Endpoint = endpoint
			},
		},
		{
			name: "endpoint too long for a unix socket",
			setup: func(t *testing.T, netAddr *confignet.AddrConfig) {
				netAddr.Endpoint = filepath.Join(t.TempDir(), strings.Repeat("x", 108)+".sock")
			},
			expectErr: "failed to listen on endpoint",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			cfg, ok := defaultConfig().(*Config)
			require.True(t, ok)
			tt.setup(t, &cfg.NetAddr)

			e := newXpuInfoExporter(cfg, exportertest.NewNopSettings(typ))
			err := e.start(context.Background(), componenttest.NewNopHost())

			if tt.expectErr != "" {
				assert.ErrorContains(t, err, tt.expectErr)
				return
			}

			require.NoError(t, err)
			assert.NoError(t, e.shutdown(context.Background()))
		})
	}
}

// TestMultipleStart verifies that a second start is a no-op, (both the metrics and the
// logs pipeline start the same shared exporter).
func TestMultipleStart(t *testing.T) {
	cfg, ok := defaultConfig().(*Config)
	require.True(t, ok)
	cfg.NetAddr.Endpoint = filepath.Join(t.TempDir(), "xpuinfo.sock")

	core, logs := observer.New(zap.InfoLevel)
	settings := exportertest.NewNopSettings(typ)
	settings.Logger = zap.New(core)
	e := newXpuInfoExporter(cfg, settings)

	require.NoError(t, e.start(context.Background(), componenttest.NewNopHost()))
	require.NoError(t, e.start(context.Background(), componenttest.NewNopHost()))
	assert.Equal(t, 1, logs.FilterMessage("Starting XPU Info exporter").Len())
	assert.Equal(t, 1, logs.FilterMessage("gRPC server listening").Len())

	require.NoError(t, e.shutdown(context.Background()))
	// Shutting down twice must be safe, too.
	assert.NoError(t, e.shutdown(context.Background()))
	assert.Equal(t, 1, logs.FilterMessage("Shutting down XPU Info exporter").Len())
	assert.Equal(t, 1, logs.FilterMessage("gRPC server stopped gracefully").Len())
}
