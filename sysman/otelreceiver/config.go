//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package otelreceiver

import (
	"fmt"
	"time"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/scraper/scraperhelper"
	"go.uber.org/zap/zapcore"
)

// Config defines configuration for the sysman receiver
type Config struct {
	scraperhelper.ControllerConfig `mapstructure:",squash"`
	SamplingInterval               time.Duration `mapstructure:"sampling_interval"`
	LogLevel                       zapcore.Level `mapstructure:"log_level"`
}

func defaultConfig() component.Config {
	return &Config{
		ControllerConfig: scraperhelper.NewDefaultControllerConfig(),
		LogLevel:         zapcore.InfoLevel,
		SamplingInterval: 1 * time.Second,
	}
}

// Validate checks if the receiver configuration is valid
func (c *Config) Validate() error {
	if err := c.ControllerConfig.Validate(); err != nil {
		return err
	}

	if c.CollectionInterval < time.Second {
		return fmt.Errorf("collection_interval too short (%s), must be at least 1 second", c.CollectionInterval)
	}
	if c.SamplingInterval < time.Millisecond {
		return fmt.Errorf("sampling_interval too short (%s), must be at least 1ms", c.SamplingInterval)
	}
	if c.CollectionInterval/c.SamplingInterval > 1e5 {
		// Cap to 100k samples per collection cycle to avoid excessive memory use
		return fmt.Errorf("too many samples per collection cycle (%d), maximum is 1e5", c.CollectionInterval/c.SamplingInterval)
	}
	return nil
}
