//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"log/slog"
	"net/http"
	"os"
	"os/signal"

	promclient "github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"go.opentelemetry.io/otel/exporters/prometheus"
	sdkmetric "go.opentelemetry.io/otel/sdk/metric"
	"go.opentelemetry.io/otel/sdk/resource"
	semconv "go.opentelemetry.io/otel/semconv/v1.26.0"

	"github.com/intel/xpu-exporter/sysman"
)

func main() {
	port := flag.Int("port", 8080, "Port to expose metrics on")
	flag.Parse()

	slog.Info("xpu exporter starting")
	ctx, cancel := signal.NotifyContext(context.Background(), os.Interrupt)
	defer cancel()

	// Create Prometheus exporter
	// NOTE: for now use a custom registry to avoid default go and http metrics
	promRegistry := promclient.NewRegistry()
	promExporter, err := prometheus.New(prometheus.WithRegisterer(promRegistry))
	if err != nil {
		log.Fatalf("failed to create prometheus exporter: %v", err)
	}

	// Initialize OTEL
	otelResource := resource.NewWithAttributes(
		semconv.SchemaURL,
		semconv.ServiceName("xpu-exporter"),
		semconv.ServiceVersion("v0.0.0"),
		semconv.TelemetrySDKLanguageGo,
	)
	meterProvider := sdkmetric.NewMeterProvider(
		sdkmetric.WithResource(otelResource),
		sdkmetric.WithReader(promExporter),
	)
	defer func() {
		if err := meterProvider.Shutdown(ctx); err != nil {
			log.Fatalf("failed to shut down meter proviader: %v", "error")
		}
	}()

	meter := meterProvider.Meter("sysman")

	// Initialize Sysman
	s, err := sysman.New()
	if err != nil {
		log.Fatalf("failed to initialize sysman API: %v", err)
	}
	if err := s.RegisterMetrics(meter); err != nil {
		log.Fatalf("failed to register metrics: %v", err)
	}

	// Start HTTP server
	mux := http.NewServeMux()
	mux.Handle("/metrics", promhttp.HandlerFor(promRegistry, promhttp.HandlerOpts{}))
	server := http.Server{
		Addr:    fmt.Sprintf(":%d", *port),
		Handler: mux,
	}
	go func() {
		slog.Info("starting http server", "port", *port)
		if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("http server failed: %v", err)
		}
	}()

	// Wait for shutdown signal
	<-ctx.Done()
	slog.Info("xpu exporter shutting down")
	if err := server.Shutdown(context.Background()); err != nil {
		log.Fatalf("failed to shutdown http server: %v", err)
	}
}
