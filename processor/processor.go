//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package processor

import (
	"context"

	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.opentelemetry.io/collector/processor"
	"go.uber.org/zap"
)

type xpuHealthProcessor struct {
	cfg      *Config
	logger   *zap.SugaredLogger
	settings processor.Settings
}

func newXpuHealthProcessor(cfg *Config, settings processor.Settings) *xpuHealthProcessor {
	return &xpuHealthProcessor{
		cfg:      cfg,
		logger:   settings.Logger.Sugar(),
		settings: settings,
	}
}

func (x *xpuHealthProcessor) processMetrics(_ context.Context, md pmetric.Metrics) (pmetric.Metrics, error) {
	return md, nil
}
