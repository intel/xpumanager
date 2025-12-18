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
	SampleInterval  time.Duration  `json:"sampleInterval"  yaml:"sampleInterval"`
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
		SampleInterval:  1 * time.Second,
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
	if c.CollectInterval < 2*c.SampleInterval {
		orig := c.SampleInterval
		c.SampleInterval = c.CollectInterval / 2
		slog.Warn("collectInterval must be >= 2 * sampleInterval to guarantee data integrity, sampleInterval adjusted", "collectInterval", c.CollectInterval, "sampleInterval", c.SampleInterval, "oldSampleInterval", orig)
	}
	if c.SampleInterval < time.Millisecond {
		return fmt.Errorf("sampleInterval too short (%s), must be at least 1ms", c.SampleInterval)
	} else if c.SampleInterval < 100*time.Millisecond {
		slog.Warn("short sampleInterval may cause high CPU usage", "sampleInterval", c.SampleInterval)
	}
	if c.CollectInterval/c.SampleInterval > 1e5 {
		// Cap to 100k samples per collection cycle to avoid excessive memory use
		return fmt.Errorf("too many samples per collection cycle (%d), maximum is 1e5", c.CollectInterval/c.SampleInterval)
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
