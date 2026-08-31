//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package inteldevicewatch

import (
	"context"
	"fmt"
	"time"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/extension"

	"github.com/intel/xpumanager/xpumd/extension/inteldevicewatch/internal/metadata"
	"github.com/intel/xpumanager/xpumd/extension/inteldevicewatch/internal/sysdev"
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

func createDefaultConfig() component.Config {
	return &Config{
		ScanInterval:   30 * time.Second,
		SettleScans:    2,
		ChangeAction:   ActionLog,
		ReportInterval: 10 * time.Minute,
		MaxRestarts:    3,
		RestartWindow:  10 * time.Minute,
		// StateFile has no default: rate limiting needs a path the deployment knows
		StateFile:  "",
		Subsystems: []string{string(sysdev.SubsystemDRM), string(sysdev.SubsystemMEI)},
		VendorIDs:  []string{sysdev.VendorIDIntel},
		SysfsRoot:  "/sys",
		DevRoot:    "/dev",
	}
}

func createExtension(_ context.Context, settings extension.Settings, cfg component.Config) (extension.Extension, error) {
	c, ok := cfg.(*Config)
	if !ok {
		return nil, fmt.Errorf("invalid config type: %T", cfg)
	}
	return newDeviceWatch(settings, c)
}
