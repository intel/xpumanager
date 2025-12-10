//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package main

import (
	"fmt"
	"log/slog"
	"maps"
	"os"
	"slices"
	"time"

	"go.yaml.in/yaml/v3"
)

type config struct {
	CollectInterval time.Duration  `json:"collectInterval" yaml:"collectInterval"`
	LogLevel        logLevel       `json:"logLevel"        yaml:"logLevel"`
	Exporters       exporterConfig `json:"exporters"       yaml:"exporters"`
}

type exporterConfig struct {
	Stdout     basicExporterConfig `json:"stdout"          yaml:"stdout"`
	Prometheus basicExporterConfig `json:"prometheus"      yaml:"prometheus"`
	Grpc       otlpExporterConfig  `json:"grpc"            yaml:"grpc"`
	Http       otlpExporterConfig  `json:"http"            yaml:"http"`
}

type basicExporterConfig struct {
	Enabled bool `json:"enabled" yaml:"enabled"`
}

type otlpExporterConfig struct {
	Enabled  bool   `json:"enabled"  yaml:"enabled"`
	Endpoint string `json:"endpoint" yaml:"endpoint"`
	Insecure bool   `json:"insecure" yaml:"insecure"`
}

type logLevel string

func defaultConfig() *config {
	return &config{
		LogLevel:        "info",
		CollectInterval: 30 * time.Second,
		Exporters: exporterConfig{
			Stdout: basicExporterConfig{
				Enabled: true,
			},
		},
	}
}

func (c *config) validate() error {
	if c.CollectInterval < time.Second {
		return fmt.Errorf("collectInterval too short (%s), must be at least 1 second", c.CollectInterval)
	}
	if err := c.LogLevel.validate(); err != nil {
		return err
	}
	if err := c.Exporters.validate(); err != nil {
		return err
	}
	return nil
}

func (c *config) loadFromFile(path string) error {
	f, err := os.Open(path)
	if err != nil {
		return err
	}
	defer f.Close() //nolint:errcheck // return value is not checked

	decoder := yaml.NewDecoder(f)
	decoder.KnownFields(true)

	return decoder.Decode(&c)
}

func (c *exporterConfig) validate() error {
	return nil
}

var logLevels = map[string]slog.Level{
	"debug": slog.LevelDebug,
	"info":  slog.LevelInfo,
	"warn":  slog.LevelWarn,
	"error": slog.LevelError,
}

func (l *logLevel) validate() error {
	if _, ok := logLevels[string(*l)]; !ok {
		return fmt.Errorf("invalid log level: %s (must be one of %v)", *l, slices.Collect(maps.Keys(logLevels)))
	}
	return nil
}

func (l *logLevel) toSlogLevel() slog.Level {
	// Defaults to 0 (slog.LevelInfo)
	return logLevels[string(*l)]
}
