//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpustatus

import (
	"context"
	"fmt"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/processor"
	"go.opentelemetry.io/collector/processor/processorhelper"

	"github.com/intel/xpumanager/xpumd/processor/intelxpustatus/internal/metadata"
)

// NewFactory returns a factory for the processor.
func NewFactory() processor.Factory {
	return processor.NewFactory(
		metadata.Type,
		defaultConfig,
		processor.WithMetrics(createMetricsProcessor, metadata.MetricsStability))
}

func createMetricsProcessor(ctx context.Context, settings processor.Settings, pcfg component.Config, nextConsumer consumer.Metrics,
) (processor.Metrics, error) {
	cfg, ok := pcfg.(*Config)
	if !ok {
		return nil, fmt.Errorf("invalid config type: %T", pcfg)
	}

	cfg.setDefaults()

	p := newXpuHealthProcessor(cfg, settings)

	return processorhelper.NewMetrics(
		ctx,
		settings,
		pcfg,
		nextConsumer,
		p.processMetrics,
		processorhelper.WithCapabilities(consumer.Capabilities{MutatesData: true}))
}
