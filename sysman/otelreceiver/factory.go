//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package otelreceiver

import (
	"context"
	"fmt"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/receiver"
)

// NewFactory creates a factory for the sysman receiver
func NewFactory() receiver.Factory {
	const stability = component.StabilityLevelDevelopment
	return receiver.NewFactory(
		component.MustNewType("sysman"),
		defaultConfig,
		receiver.WithMetrics(createReceiver, stability),
	)
}

func createReceiver(_ context.Context, settings receiver.Settings, cfg component.Config, consumer consumer.Metrics) (receiver.Metrics, error) {
	rcfg, ok := cfg.(*Config)
	if !ok {
		return nil, fmt.Errorf("invalid config type: %T", cfg)
	}
	return newSysmanReceiver(settings, rcfg, consumer)
}
