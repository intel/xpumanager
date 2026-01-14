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
	SampleInterval                 time.Duration `mapstructure:"sample_interval"`
	LogLevel                       zapcore.Level `mapstructure:"log_level"`
}

func defaultConfig() component.Config {
	return &Config{
		ControllerConfig: scraperhelper.NewDefaultControllerConfig(),
		LogLevel:         zapcore.InfoLevel,
		SampleInterval:   1 * time.Second,
	}
}

// Validate checks if the receiver configuration is valid
func (c *Config) Validate() error {
	if err := c.ControllerConfig.Validate(); err != nil {
		return err
	}

	if c.CollectionInterval < time.Second {
		return fmt.Errorf("collectionInterval too short (%s), must be at least 1 second", c.CollectionInterval)
	}
	if c.SampleInterval < time.Millisecond {
		return fmt.Errorf("sampleInterval too short (%s), must be at least 1ms", c.SampleInterval)
	}
	if c.CollectionInterval/c.SampleInterval > 1e5 {
		// Cap to 100k samples per collection cycle to avoid excessive memory use
		return fmt.Errorf("too many samples per collection cycle (%d), maximum is 1e5", c.CollectionInterval/c.SampleInterval)
	}
	return nil
}
