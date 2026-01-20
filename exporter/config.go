//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package exporter

import "go.opentelemetry.io/collector/component"

// Config defines configuration for the exporter.
type Config struct {
}

// Validate checks if the configuration is valid.
func (cfg *Config) Validate() error {
	return nil
}

// defaultConfig provides default configuration values.
func defaultConfig() component.Config {
	return &Config{}
}
