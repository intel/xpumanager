//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpuinfo

import (
	"context"
	"errors"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"sync"
	"syscall"
	"time"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/config/confignet"
	"go.opentelemetry.io/collector/exporter"
	"go.opentelemetry.io/collector/pdata/plog"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.uber.org/zap"
	"google.golang.org/grpc"

	pb "github.com/intel/xpumanager/xpumd/exporter/intelxpuinfo/api/deviceinfo/v1alpha1"
	"github.com/intel/xpumanager/xpumd/exporter/intelxpuinfo/internal/metadata"
)

type xpuInfoExporter struct {
	sync.Mutex

	cfg        *Config
	logger     *zap.SugaredLogger
	telemetry  *metadata.TelemetryBuilder
	settings   exporter.Settings
	grpcServer *grpc.Server
	infoServer *deviceInfoServer

	started bool
}

func newXpuInfoExporter(cfg *Config, settings exporter.Settings) *xpuInfoExporter {
	return &xpuInfoExporter{
		cfg:      cfg,
		logger:   settings.Logger.Sugar(),
		settings: settings,
	}
}

func (e *xpuInfoExporter) start(ctx context.Context, host component.Host) error {
	e.Lock()
	defer e.Unlock()

	if e.started {
		return nil
	}

	e.logger.Info("Starting XPU Info exporter")

	tb, err := metadata.NewTelemetryBuilder(e.settings.TelemetrySettings)
	if err != nil {
		return err
	}
	e.telemetry = tb

	if err := e.startGrpcServer(ctx); err != nil {
		return fmt.Errorf("failed to start gRPC server: %w", err)
	}

	e.started = true
	return nil
}

func (e *xpuInfoExporter) shutdown(ctx context.Context) error {
	const shutdownTimeout = 5 * time.Second

	e.Lock()
	defer e.Unlock()

	if !e.started {
		return nil
	}
	e.started = false

	e.logger.Info("Shutting down XPU Info exporter")

	ctx, cancel := context.WithTimeout(ctx, shutdownTimeout)
	defer cancel()

	done := make(chan struct{})
	go func() {
		if e.infoServer != nil {
			e.infoServer.stop()
		}
		if e.grpcServer != nil {
			e.grpcServer.GracefulStop()
		}
		close(done)
	}()

	select {
	case <-done:
		e.logger.Info("gRPC server stopped gracefully")
	case <-ctx.Done():
		// Force stop
		if e.grpcServer != nil {
			e.grpcServer.Stop()
		}
		return fmt.Errorf("gRPC server shutdown timed out: %v", ctx.Err())
	}
	return nil
}

func (e *xpuInfoExporter) pushMetrics(_ context.Context, md pmetric.Metrics) error {
	e.logger.Debugw("Pushing metrics", "dataPoints", md.DataPointCount())

	// NOTE: No batching is in place. We rely on the property that md contains
	// all metrics for a particular device.
	e.infoServer.updateHealthData(md)

	return nil
}

func (e *xpuInfoExporter) pushLogs(_ context.Context, ld plog.Logs) error {
	e.logger.Debugw("Pushing logs", "logRecords", ld.LogRecordCount())
	e.infoServer.broadcastEvents(ld)
	return nil
}

func (e *xpuInfoExporter) startGrpcServer(ctx context.Context) error {
	grpcServer, err := e.cfg.ToServer(ctx, nil, e.settings.TelemetrySettings)
	if err != nil {
		return fmt.Errorf("failed to create gRPC server: %v", err)
	}

	infoServer := newDeviceInfoServer(e.logger, e.telemetry, e.cfg)
	pb.RegisterDeviceInfoServer(grpcServer, infoServer)

	if e.cfg.NetAddr.Transport == confignet.TransportTypeUnix {
		endpoint := e.cfg.NetAddr.Endpoint
		if endpoint == "" {
			// Use runtime dir specified for the user?
			name := metadata.Type.String() + ".sock"
			if dir := os.Getenv("XDG_RUNTIME_DIR"); dir != "" {
				endpoint = filepath.Join(dir, name)
			} else {
				return fmt.Errorf("neither XDG_RUNTIME_DIR set, nor intelxpuinfo.endpoint configured")
			}
		}

		dir := filepath.Dir(endpoint)
		if stat, err := os.Stat(dir); errors.Is(err, os.ErrNotExist) {
			// Create dir if it does not exist
			if err := os.MkdirAll(dir, 0o750); err != nil {
				return fmt.Errorf("failed to create directory for unix socket: %w", err)
			}
		} else if err != nil {
			return fmt.Errorf("failed to stat unix socket directory: %w", err)
		} else if !stat.IsDir() {
			return fmt.Errorf("configured unix socket dir is not a directory: %s", dir)
		}

		// Remove the socket file if it already exists
		if stat, err := os.Lstat(endpoint); err == nil {
			if stat.Mode()&fs.ModeSocket == 0 {
				// This is privileged process, remove only files of correct type
				return fmt.Errorf("refusing to remove existing/stale endpoint, it's not a socket: %s", endpoint)
			}
			if err = os.Remove(endpoint); err != nil && !errors.Is(err, os.ErrNotExist) {
				return fmt.Errorf("failed to remove existing/stale unix socket file: %w", err)
			}
		} else if !errors.Is(err, os.ErrNotExist) {
			return fmt.Errorf("failed to stat existing unix socket endpoint: %w", err)
		}
		e.cfg.NetAddr.Endpoint = endpoint
	}

	// Socket writable by the same user or group ID (0o117 mask => 0o660 access)
	umask := syscall.Umask(0o117)
	lis, err := e.cfg.NetAddr.Listen(ctx)
	syscall.Umask(umask)
	if err != nil {
		return fmt.Errorf("failed to listen on endpoint: %w", err)
	}

	e.logger.Infow("gRPC server listening", "endpoint", e.cfg.NetAddr.Endpoint)
	e.grpcServer = grpcServer
	e.infoServer = infoServer

	go func() {
		if err := grpcServer.Serve(lis); err != nil {
			e.logger.Errorw("gRPC server failed", "error", err)
		}
	}()
	return nil
}
