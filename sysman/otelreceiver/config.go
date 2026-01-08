//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package otelreceiver

import (
	"fmt"
	"time"

	"go.opentelemetry.io/collector/component"
	"go.uber.org/zap/zapcore"
)

// Config defines configuration for the sysman receiver
type Config struct {
	CollectInterval time.Duration `mapstructure:"collect_interval"`
	SampleInterval  time.Duration `mapstructure:"sample_interval"`
	LogLevel        zapcore.Level `mapstructure:"log_level"`
}

func defaultConfig() component.Config {
	return &Config{
		LogLevel:        zapcore.InfoLevel,
		CollectInterval: 30 * time.Second,
		SampleInterval:  1 * time.Second,
	}
}

// Validate checks if the receiver configuration is valid
func (c *Config) Validate() error {
	if c.CollectInterval < time.Second {
		return fmt.Errorf("collectInterval too short (%s), must be at least 1 second", c.CollectInterval)
	}
	if c.SampleInterval < time.Millisecond {
		return fmt.Errorf("sampleInterval too short (%s), must be at least 1ms", c.SampleInterval)
	}
	if c.CollectInterval/c.SampleInterval > 1e5 {
		// Cap to 100k samples per collection cycle to avoid excessive memory use
		return fmt.Errorf("too many samples per collection cycle (%d), maximum is 1e5", c.CollectInterval/c.SampleInterval)
	}
	return nil
}
