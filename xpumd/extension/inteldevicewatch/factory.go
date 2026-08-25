//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package inteldevicewatch

import (
	"context"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/extension"

	"github.com/intel/xpumanager/xpumd/extension/inteldevicewatch/internal/metadata"
)

// NewFactory creates a factory for the extension.
func NewFactory() extension.Factory {
	return extension.NewFactory(
		metadata.Type,
		createDefaultConfig,
		createExtension,
		metadata.ExtensionStability,
	)
}

// Config defines configuration for the Intel device watch extension.
type Config struct{}

func createDefaultConfig() component.Config {
	return &Config{}
}

// deviceWatch is a placeholder that does nothing, yet.
type deviceWatch struct {
	component.StartFunc
	component.ShutdownFunc
}

func createExtension(_ context.Context, _ extension.Settings, _ component.Config) (extension.Extension, error) {
	return &deviceWatch{}, nil
}
