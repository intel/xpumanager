//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package exporter

import (
	"fmt"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/config/configgrpc"
	"go.opentelemetry.io/collector/config/confignet"
)

// Config defines configuration for the exporter.
type Config struct {
	configgrpc.ServerConfig `mapstructure:",squash"`
}

// Validate checks if the configuration is valid.
func (cfg *Config) Validate() error {
	if cfg.NetAddr.Transport != confignet.TransportTypeUnix {
		return fmt.Errorf("unsupported transport type: %q, only %q is supported", cfg.NetAddr.Transport, confignet.TransportTypeUnix)
	}
	return nil
}

// defaultConfig provides default configuration values.
func defaultConfig() component.Config {
	cfg := &Config{
		ServerConfig: configgrpc.NewDefaultServerConfig(),
	}

	// Set transport to Unix Domain Sockets by default.
	cfg.NetAddr.Transport = confignet.TransportTypeUnix

	return cfg
}
