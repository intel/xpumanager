//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpu

import (
	"context"
	"fmt"
	"sync"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/consumer"
	"go.opentelemetry.io/collector/receiver"
	"go.opentelemetry.io/collector/scraper/scraperhelper"

	"github.com/intel/xpumanager/xpumd/receiver/intelxpu/internal/metadata"
	"github.com/intel/xpumanager/xpumd/receiver/intelxpu/sysman"
)

var (
	sysmanFactory = sysman.New()
	instances     instanceGuard
)

// instanceGuard permits only one receiver instance (component ID) per process.
// The intelxpu/sysman package is not designed to support multiple instances as
// it maintains a global singleton state (of devices and configuration).
//
// NOTE: A single instance (ID) is created twice when used in both a metrics and a
// logs pipeline (as in config-example.yaml), which is supported. Only other
// differently named instances are rejected.
type instanceGuard struct {
	sync.Mutex
	id *component.ID
}

func (g *instanceGuard) claim(id component.ID) error {
	g.Lock()
	defer g.Unlock()

	if g.id != nil && *g.id != id {
		return fmt.Errorf("cannot create receiver %q: multiple %s instances not supported, %q already exists", id, metadata.Type, *g.id)
	}
	g.id = &id
	return nil
}

// NewFactory creates a factory for the receiver.
func NewFactory() receiver.Factory {
	return receiver.NewFactory(
		metadata.Type,
		defaultConfig,
		receiver.WithMetrics(createMetricsReceiver, metadata.MetricsStability),
		receiver.WithLogs(createLogsReceiver, metadata.LogsStability),
	)
}

func createMetricsReceiver(_ context.Context, settings receiver.Settings, rcfg component.Config, consumer consumer.Metrics) (receiver.Metrics, error) {
	cfg, ok := rcfg.(*Config)
	if !ok {
		return nil, fmt.Errorf("invalid config type: %T", rcfg)
	}

	if err := instances.claim(settings.ID); err != nil {
		return nil, err
	}

	return scraperhelper.NewMetricsController(
		&cfg.ControllerConfig,
		settings,
		consumer,
		scraperhelper.AddFactoryWithConfig(sysmanFactory, cfg.Config),
	)
}

func createLogsReceiver(_ context.Context, settings receiver.Settings, rcfg component.Config, nextConsumer consumer.Logs) (receiver.Logs, error) {
	cfg, ok := rcfg.(*Config)
	if !ok {
		return nil, fmt.Errorf("invalid config type: %T", rcfg)
	}

	if err := instances.claim(settings.ID); err != nil {
		return nil, err
	}

	return sysmanFactory.CreateLogsReceiver(settings.TelemetrySettings, cfg.Config, nextConsumer)
}
