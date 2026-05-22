//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"fmt"
	"time"

	"github.com/intel/xpumanager/xpumd/receiver/intelxpu/sysman/internal/metadata"
	"go.opentelemetry.io/collector/component"
)

// Config defines configuration for the Sysman scraper.
type Config struct {
	metadata.MetricsBuilderConfig `mapstructure:",squash"`
	SamplingInterval              time.Duration `mapstructure:"sampling_interval"`

	aggregatedMetricsBufferSize int
}

// defaultConfig creates the default configuration for the scraper.
func defaultConfig() component.Config {
	return &Config{
		MetricsBuilderConfig: metadata.NewDefaultMetricsBuilderConfig(),
		SamplingInterval:     1 * time.Second,
	}
}

// Validate checks if the receiver configuration is valid.
func (c *Config) Validate() error {
	if c.SamplingInterval < time.Millisecond {
		return fmt.Errorf("sampling_interval too short (%s), must be at least 1ms", c.SamplingInterval)
	}
	return nil
}

func (c *Config) SetAggregatedMetricsBufferSize(size int) {
	c.aggregatedMetricsBufferSize = size
}
