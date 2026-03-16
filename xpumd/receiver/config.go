//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package receiver

import (
	"fmt"
	"time"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/scraper/scraperhelper"
	"go.uber.org/zap"

	"github.com/intel/xpumanager/xpumd/receiver/sysman"
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
func (c *Config) Validate() error {
	if err := c.ControllerConfig.Validate(); err != nil {
		return err
	}

	if c.CollectionInterval < time.Second {
		return fmt.Errorf("collection_interval too short (%s), must be at least 1 second", c.CollectionInterval)
	}

	if c.CollectionInterval/c.SamplingInterval > 1e5 {
		// Cap to 100k samples per collection cycle to avoid excessive memory use
		return fmt.Errorf("too many samples per collection cycle (%d), maximum is 1e5", c.CollectionInterval/c.SamplingInterval)
	}
	return nil
}

func (c *Config) lint(logger *zap.SugaredLogger) {
	// Warn about potentially config issues.
	// Cannot do in Config.Validate() as logger is not available there.
	if c.CollectionInterval < 2*c.SamplingInterval {
		orig := c.SamplingInterval
		c.SamplingInterval = c.CollectionInterval / 2
		logger.Warnw("collection_interval must be >= 2 * sampling_interval to guarantee data integrity, sampling_interval adjusted",
			"collectionInterval", c.CollectionInterval,
			"samplingInterval", c.SamplingInterval,
			"oldSamplingInterval", orig,
		)
	}
}
