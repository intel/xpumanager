package main

import (
	"context"
	"fmt"
	"log/slog"
	"sync/atomic"
	"time"

	"go.opentelemetry.io/otel/exporters/otlp/otlpmetric/otlpmetricgrpc"
	"go.opentelemetry.io/otel/exporters/otlp/otlpmetric/otlpmetrichttp"
	"go.opentelemetry.io/otel/exporters/stdout/stdoutmetric"
	sdkmetric "go.opentelemetry.io/otel/sdk/metric"
	"go.opentelemetry.io/otel/sdk/metric/metricdata"
)

type deviceMonitor struct {
	cfg              *config
	metricsReader    *sdkmetric.ManualReader
	metricsExporters []*metricsExporter
}

type metricsExporter struct {
	sdkmetric.Exporter

	name       string
	inProgress atomic.Bool
}

func newMetricsExporter(name string, exporter sdkmetric.Exporter) *metricsExporter {
	return &metricsExporter{
		name:     name,
		Exporter: exporter,
	}
}

func newDeviceMonitor(cfg *config) (*deviceMonitor, error) {
	m := &deviceMonitor{
		cfg:           cfg,
		metricsReader: sdkmetric.NewManualReader(),
	}

	// Initialize (push-based) exporters
	if cfg.Exporters.Stdout.Enabled {
		exporter, err := stdoutmetric.New(stdoutmetric.WithPrettyPrint())
		if err != nil {
			return nil, fmt.Errorf("failed to create stdout metrics exporter: %w", err)
		}
		slog.Info("stdout metrics exporter enabled")
		m.metricsExporters = append(m.metricsExporters, newMetricsExporter("stdout", exporter))
	}

	if c := cfg.Exporters.Grpc; c.Enabled {
		opts := []otlpmetricgrpc.Option{}
		if c.Endpoint != "" {
			opts = append(opts, otlpmetricgrpc.WithEndpoint(c.Endpoint))
		}
		if c.Insecure {
			opts = append(opts, otlpmetricgrpc.WithInsecure())
		}
		exporter, err := otlpmetricgrpc.New(context.TODO(), opts...)
		if err != nil {
			return nil, fmt.Errorf("failed to create OTLP/gRPC metrics exporter: %w", err)
		}
		slog.Info("OTLP/gRPC metrics exporter enabled")
		m.metricsExporters = append(m.metricsExporters, newMetricsExporter("otlp-grpc", exporter))
	}

	if c := cfg.Exporters.Http; c.Enabled {
		opts := []otlpmetrichttp.Option{}
		if c.Endpoint != "" {
			opts = append(opts, otlpmetrichttp.WithEndpoint(c.Endpoint))
		}
		if c.Insecure {
			opts = append(opts, otlpmetrichttp.WithInsecure())
		}
		exporter, err := otlpmetrichttp.New(context.TODO(), opts...)
		if err != nil {
			return nil, fmt.Errorf("failed to create OTLP/HTTP metrics exporter: %w", err)
		}
		slog.Info("OTLP/HTTP metrics exporter enabled")
		m.metricsExporters = append(m.metricsExporters, newMetricsExporter("otlp-http", exporter))
	}

	return m, nil
}

// run is the main loop for the device monitor.
func (m *deviceMonitor) run(ctx context.Context) error {
	ticker := time.NewTicker(m.cfg.CollectInterval)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			slog.Info("collecting device metrics")
			var rm metricdata.ResourceMetrics
			if err := m.metricsReader.Collect(ctx, &rm); err != nil {
				return fmt.Errorf("failed to collect metrics: %w", err)
			}

			for _, exporter := range m.metricsExporters {
				if !exporter.inProgress.CompareAndSwap(false, true) {
					slog.Info("skipping metrics export: previous export still in progress", "exporter", exporter.name)
					continue
				}
				slog.Debug("exporting device metrics", "exporter", exporter.name)
				go func(e *metricsExporter) {
					defer e.inProgress.Store(false)
					if err := e.Export(ctx, &rm); err != nil {
						slog.Error("failed to export metrics", "error", err, "exporter", e.name)
					}
				}(exporter)
			}
		case <-ctx.Done():
			return nil

		}
	}
}
