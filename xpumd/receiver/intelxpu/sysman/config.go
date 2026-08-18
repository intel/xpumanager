//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"errors"
	"fmt"
	"strings"
	"time"

	"github.com/intel/xpumanager/xpumd/receiver/intelxpu/sysman/internal/metadata"
	"go.opentelemetry.io/collector/component"
)

// Config defines configuration for the Sysman scraper.
type Config struct {
	metadata.MetricsBuilderConfig `mapstructure:",squash"`
	SamplingInterval              time.Duration `mapstructure:"sampling_interval"`
	// FailOnSysmanInitError controls whether a failure to initialize the L0
	// Sysman API (e.g. no GPU driver or device access) causes the collector to
	// fail at startup.
	FailOnSysmanInitError bool `mapstructure:"fail_on_sysman_init_error"`
	// InfoLogs configures the collection of the GPU info logs.
	InfoLogs InfoLogsConfig `mapstructure:"info_logs"`

	aggregatedMetricsBufferSize int
}

// InfoLogsConfig defines configuration for collecting the GPU info logs, i.e.
// the CPER records provided by the GPU driver.
type InfoLogsConfig struct {
	// Enabled controls whether the info logs are collected.
	Enabled bool `mapstructure:"enabled"`
	// InstanceName is the name of the collection instance to be used. Required.
	InstanceName string `mapstructure:"instance_name"`
}

// defaultInstanceName is the default name of the info log collection instance.
const defaultInstanceName = "xpumd"

// defaultConfig creates the default configuration for the scraper.
func defaultConfig() component.Config {
	return &Config{
		MetricsBuilderConfig: metadata.NewDefaultMetricsBuilderConfig(),
		SamplingInterval:     1 * time.Second,
		InfoLogs: InfoLogsConfig{
			InstanceName: defaultInstanceName,
		},
	}
}

// Validate checks if the receiver configuration is valid.
func (c *Config) Validate() error {
	if c.SamplingInterval < time.Millisecond {
		return fmt.Errorf("sampling_interval too short (%s), must be at least 1ms", c.SamplingInterval)
	}
	// Prevent usage of the default/global buffer of the driver as it is likely shared with any other consumer of it
	// (e.g. the global tracefs buffer in case of CPER records).
	if c.InfoLogs.InstanceName == "" {
		return errors.New("empty info_logs.instance_name, the name of the collection instance is required")
	}
	// The driver may use the name as a path component, e.g. a tracefs instance directory
	// NOTE: the API (spec) does not restrict this but better be safe than sorry and avoid any potential issues.
	if strings.ContainsAny(c.InfoLogs.InstanceName, "/") {
		return fmt.Errorf("invalid info_logs.instance_name (%q), must not contain '/'",
			c.InfoLogs.InstanceName)
	}
	return nil
}

func (c *Config) SetAggregatedMetricsBufferSize(size int) {
	c.aggregatedMetricsBufferSize = size
}
