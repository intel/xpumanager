//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package processor

import "go.opentelemetry.io/collector/component"

// Config defines configuration for the processor.
type Config struct {
}

func defaultConfig() component.Config {
	return &Config{}
}

// Validate checks if the processor configuration is valid.
func (c *Config) Validate() error {
	return nil
}
