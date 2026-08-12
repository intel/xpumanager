//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpu

import (
	"fmt"
	"time"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/scraper/scraperhelper"

	"github.com/intel/xpumanager/xpumd/receiver/intelxpu/sysman"
)

// Config defines configuration for the receiver.
type Config struct {
	scraperhelper.ControllerConfig `mapstructure:",squash"`
	*sysman.Config                 `mapstructure:",squash"`
}

func defaultConfig() component.Config {
	return &Config{
		ControllerConfig: scraperhelper.NewDefaultControllerConfig(),
		Config:           sysmanFactory.CreateDefaultConfig().(*sysman.Config),
	}
}

// Validate checks if the receiver configuration is valid.
// NOTE: Does not validate the nested fields implementing Validate(). The
// method is expected to be called by the otel-collector validator
// (confmap.Validate()) which recursively validates the nested types.
func (c *Config) Validate() error {
	if c.CollectionInterval < time.Second {
		return fmt.Errorf("collection_interval too short (%s), must be at least 1 second", c.CollectionInterval)
	}

	samplingInterval := c.SamplingInterval
	if samplingInterval <= 0 {
		return fmt.Errorf("sampling_interval (%v) must be greater than zero", samplingInterval)
	}
	if c.CollectionInterval < 2*samplingInterval {
		return fmt.Errorf("collection_interval (%s) must be at least twice the sampling_interval (%s)", c.CollectionInterval, samplingInterval)
	}

	if c.CollectionInterval/samplingInterval > 1e5 {
		// Cap to 100k samples per collection cycle to avoid excessive memory use
		return fmt.Errorf("too many samples per collection cycle (%d), maximum is 1e5", c.CollectionInterval/samplingInterval)
	}

	// Reserve one second (or at least 10 samples) extra for the aggregated sample buffer to mitigate possible jitter to not lose samples
	const sampleBufferMinExtraSamples = 10

	// Compute derived configuration values
	samplesPerInterval := c.CollectionInterval / c.SamplingInterval
	extraSamples := max(time.Second/c.SamplingInterval, sampleBufferMinExtraSamples)
	c.SetAggregatedMetricsBufferSize(int(samplesPerInterval + extraSamples))

	return nil
}
