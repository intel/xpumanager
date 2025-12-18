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
	"go.opentelemetry.io/otel/exporters/prometheus"
	"go.opentelemetry.io/otel/metric"
	sdkmetric "go.opentelemetry.io/otel/sdk/metric"
	"go.opentelemetry.io/otel/sdk/resource"
	semconv "go.opentelemetry.io/otel/semconv/v1.26.0"

	"github.com/intel/xpu-exporter/sysman"
)

type flagsT struct {
	port       int
	configFile string
	debug      bool
}

func main() {
	flags := flagsT{}
	flag.IntVar(&flags.port, "port", 8080, "Port to expose metrics on")
	flag.StringVar(&flags.configFile, "config", "", "Path to configuration file")
	flag.BoolVar(&flags.debug, "debug", false, "Enable debug logging, overrides the logLevel option from the configuration file")
	flag.Parse()

	os.Exit(run(flags))
}

func run(flags flagsT) (exitCode int) {
	slog.Info("xpu exporter starting")
	ctx, cancel := signal.NotifyContext(context.Background(), os.Interrupt)
	defer cancel()

	// Load configuration
	cfg := defaultConfig()
	if f := flags.configFile; f != "" {
		if err := cfg.loadFromFile(f); err != nil {
			slog.Error("failed to load configuration file", "file", f, "error", err)
			return 1
		}
		slog.Info("configuration file loaded", "file", f)
	}
	if err := cfg.validate(); err != nil {
		slog.Error("invalid configuration", "error", err)
		return 1
	}

	if flags.debug {
		slog.SetLogLoggerLevel(slog.LevelDebug)
	} else {
		slog.SetLogLoggerLevel(cfg.LogLevel.toSlogLevel())
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

	shutdownMeterProvider := func(mp *sdkmetric.MeterProvider, name string) {
		if err := mp.Shutdown(ctx); err != nil {
			slog.Error("failed to shut down meter provider", "error", err, "name", name)
			exitCode = 1
		}
		slog.Info("meter provider shut down", "name", name)
	}

	// Initialize Prometheus exporter (async)
	var meterProm metric.Meter
	if cfg.Exporters.Prometheus.Enabled {
		promRegistry := promclient.NewRegistry()
		exporter, err := prometheus.New(prometheus.WithRegisterer(promRegistry))
		if err != nil {
			slog.Error("failed to create Prometheus exporter", "error", err)
			return 1
		}
		meterProviderProm := sdkmetric.NewMeterProvider(append(mpOpts, sdkmetric.WithReader(exporter))...)
		defer shutdownMeterProvider(meterProviderProm, "Prometheus")
		meterProm = meterProviderProm.Meter("sysman-prom")
		mux.Handle("/metrics", promhttp.HandlerFor(promRegistry, promhttp.HandlerOpts{}))
		slog.Info("Prometheus exporter enabled")
	}

	// Initialize Sysman
	// Reserve one second (or at least 10 samples) extra for the aggreated sample buffer to mitigate possible jitter to not lose samples
	const sampleBufferMinExtraSamples = 10
	aggregatedSampleCount := cfg.CollectInterval/cfg.SampleInterval + max(time.Second/cfg.SampleInterval, sampleBufferMinExtraSamples)

	s, err := sysman.New(int(aggregatedSampleCount))
	if err != nil {
		slog.Error("failed to initialize L0 Sysman API (likely a Level-Zero driver / device access issue)", "error", err)
		return 1
	}

	h, err := newDeviceMonitor(cfg, s)
	if err != nil {
		slog.Error("failed to create device monitor", "error", err)
		return 1
	}

	meterProvider := sdkmetric.NewMeterProvider(append(mpOpts, sdkmetric.WithReader(h.metricsReader))...)
	defer shutdownMeterProvider(meterProvider, "device-monitor")

	meter := meterProvider.Meter("sysman")

	if err := s.RegisterMetrics(meter); err != nil {
		slog.Error("failed to register sysman metrics", "error", err)
		return 1
	}
	if meterProm != nil {
		if err := s.RegisterMetrics(meterProm); err != nil {
			slog.Error("failed to register Prometheus metrics from sysman", "error", err)
			return 1
		}
	}

	// Start HTTP server
	var server *http.Server
	if cfg.Exporters.Prometheus.Enabled {
		server = &http.Server{
			Addr:    fmt.Sprintf(":%d", flags.port),
			Handler: mux,
		}
		go func() {
			slog.Info("starting HTTP server", "port", flags.port)
			if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
				slog.Error("HTTP server failed", "error", err)
				exitCode = 1
				cancel()
			}
		}()
	}

	go func() {
		if err := h.run(ctx); err != nil {
			slog.Error("device monitor failed", "error", err)
			exitCode = 1
			cancel()
		}
	}()

	// Wait for shutdown signal
	<-ctx.Done()
	slog.Info("xpu exporter shutting down")
	shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer shutdownCancel()

	if server != nil {
		if err := server.Shutdown(shutdownCtx); err != nil {
			slog.Error("failed to shutdown HTTP server", "error", err)
			exitCode = 1
		}
	}

	return exitCode
}
