//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"context"
	"flag"
	"fmt"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"time"

	promclient "github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"go.opentelemetry.io/otel/exporters/otlp/otlpmetric/otlpmetricgrpc"
	"go.opentelemetry.io/otel/exporters/otlp/otlpmetric/otlpmetrichttp"
	"go.opentelemetry.io/otel/exporters/prometheus"
	"go.opentelemetry.io/otel/exporters/stdout/stdoutmetric"
	sdkmetric "go.opentelemetry.io/otel/sdk/metric"
	"go.opentelemetry.io/otel/sdk/resource"
	semconv "go.opentelemetry.io/otel/semconv/v1.26.0"

	"github.com/intel/xpu-exporter/sysman"
)

var (
	port       = flag.Int("port", 8080, "Port to expose metrics on")
	configFile = flag.String("config", "", "Path to configuration file")
)

func main() {
	flag.Parse()

	os.Exit(run())
}

func run() (exitCode int) {
	slog.Info("xpu exporter starting")
	ctx, cancel := signal.NotifyContext(context.Background(), os.Interrupt)
	defer cancel()

	// Load configuration
	cfg := defaultConfig()
	if *configFile != "" {
		if err := cfg.loadFromFile(*configFile); err != nil {
			slog.Error("failed to load configuration file", "file", *configFile, "error", err)
			return 1
		}
		slog.Info("configuration file loaded", "file", *configFile)
	}
	if err := cfg.validate(); err != nil {
		slog.Error("invalid configuration", "error", err)
		return 1
	}

	// HTTP server mux
	mux := http.NewServeMux()

	// Initialize OTEL resource
	otelResource := resource.NewWithAttributes(
		semconv.SchemaURL,
		semconv.ServiceName("xpu-exporter"),
		semconv.ServiceVersion("v0.0.0"),
		semconv.TelemetrySDKLanguageGo,
	)
	mpOpts := []sdkmetric.Option{
		sdkmetric.WithResource(otelResource),
	}

	// Initialize exporters
	// NOTE: for the periodic exporters, each exporter gets its own reader with
	// the same collection interval. Improvement for the future, if multiple
	// exporters is a valid use case: share a single reader and push to
	// enabled exporters.
	withPeriodicReader := func(exporter sdkmetric.Exporter) sdkmetric.Option {
		return sdkmetric.WithReader(sdkmetric.NewPeriodicReader(exporter, sdkmetric.WithInterval(time.Duration(cfg.Exporters.CollectInterval)*time.Second)))
	}
	if cfg.Exporters.Stdout.Enabled {
		exporter, err := stdoutmetric.New(stdoutmetric.WithPrettyPrint())
		if err != nil {
			slog.Error("failed to create stdout metrics exporter", "error", err)
			return 1
		}
		slog.Info("stdout metrics exporter enabled")
		mpOpts = append(mpOpts, withPeriodicReader(exporter))
	}

	if cfg.Exporters.Prometheus.Enabled {
		promRegistry := promclient.NewRegistry()
		promExporter, err := prometheus.New(prometheus.WithRegisterer(promRegistry))
		if err != nil {
			slog.Error("failed to create Prometheus exporter", "error", err)
			return 1
		}
		mpOpts = append(mpOpts, sdkmetric.WithReader(promExporter))
		mux.Handle("/metrics", promhttp.HandlerFor(promRegistry, promhttp.HandlerOpts{}))
		slog.Info("Prometheus exporter enabled")
	}

	if c := cfg.Exporters.Grpc; c.Enabled {
		opts := []otlpmetricgrpc.Option{}
		if c.Endpoint != "" {
			opts = append(opts, otlpmetricgrpc.WithEndpoint(c.Endpoint))
		}
		if c.Insecure {
			opts = append(opts, otlpmetricgrpc.WithInsecure())
		}
		exporter, err := otlpmetricgrpc.New(ctx, opts...)
		if err != nil {
			slog.Error("failed to create OTLP/gRPC metrics exporter", "error", err)
			return 1
		}
		slog.Info("OTLP/gRPC metrics exporter enabled")

		mpOpts = append(mpOpts, withPeriodicReader(exporter))
	}

	if c := cfg.Exporters.Http; c.Enabled {
		opts := []otlpmetrichttp.Option{}
		if c.Endpoint != "" {
			opts = append(opts, otlpmetrichttp.WithEndpoint(c.Endpoint))
		}
		if c.Insecure {
			opts = append(opts, otlpmetrichttp.WithInsecure())
		}
		exporter, err := otlpmetrichttp.New(ctx, opts...)
		if err != nil {
			slog.Error("failed to create OTLP/HTTP metrics exporter", "error", err)
			return 1
		}
		slog.Info("OTLP/HTTP metrics exporter enabled")
		mpOpts = append(mpOpts, withPeriodicReader(exporter))
	}

	meterProvider := sdkmetric.NewMeterProvider(mpOpts...)
	defer func() {
		if err := meterProvider.Shutdown(ctx); err != nil {
			slog.Error("failed to shut down meter provider", "error", err)
			exitCode = 1
		}
		slog.Info("meter provider shut down")
	}()

	meter := meterProvider.Meter("sysman")

	// Initialize Sysman
	s, err := sysman.New()
	if err != nil {
		slog.Error("failed to initialize sysman API", "error", err)
		return 1
	}
	if err := s.RegisterMetrics(meter); err != nil {
		slog.Error("failed to register sysman metrics", "error", err)
		return 1
	}

	// Start HTTP server
	var server *http.Server
	if cfg.Exporters.Prometheus.Enabled {
		server = &http.Server{
			Addr:    fmt.Sprintf(":%d", *port),
			Handler: mux,
		}
		go func() {
			slog.Info("starting HTTP server", "port", *port)
			if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
				slog.Error("HTTP server failed", "error", err)
				exitCode = 1
				cancel()
			}
		}()
	}

	// Wait for shutdown signal
	<-ctx.Done()
	slog.Info("xpu exporter shutting down")
	if server != nil {
		if err := server.Shutdown(context.Background()); err != nil {
			slog.Error("failed to shutdown HTTP server", "error", err)
			exitCode = 1
		}
	}

	return exitCode
}
