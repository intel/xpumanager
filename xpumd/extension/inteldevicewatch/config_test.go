//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package inteldevicewatch

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/intel/xpumanager/xpumd/extension/inteldevicewatch/internal/sysdev"
)

func TestConfigValidate(t *testing.T) {
	for _, tc := range []struct {
		name    string
		mutate  func(*Config)
		wantErr string
	}{
		{
			name:   "default",
			mutate: func(*Config) {},
		},
		{
			name:   "report interval may be zero",
			mutate: func(c *Config) { c.ReportInterval = 0 },
		},
		{
			name:    "zero scan interval",
			mutate:  func(c *Config) { c.ScanInterval = 0 },
			wantErr: errScanInterval,
		},
		{
			name:    "zero settle scans",
			mutate:  func(c *Config) { c.SettleScans = 0 },
			wantErr: errSettleScans,
		},
		{
			name:    "negative report interval",
			mutate:  func(c *Config) { c.ReportInterval = -time.Second },
			wantErr: errReportInterval,
		},
		{
			name:    "no subsystems",
			mutate:  func(c *Config) { c.Subsystems = nil },
			wantErr: errNoSubsystems,
		},
		{
			name:    "unknown subsystem",
			mutate:  func(c *Config) { c.Subsystems = []string{"drm", "usb"} },
			wantErr: sysdev.ErrUnsupportedSubsystem.Error(),
		},
		{
			name:    "invalid vendor ID",
			mutate:  func(c *Config) { c.VendorIDs = []string{"8086", "nope"} },
			wantErr: sysdev.ErrInvalidVendorID.Error(),
		},
		{
			name:    "empty sysfs root",
			mutate:  func(c *Config) { c.SysfsRoot = "" },
			wantErr: errSysfsRoot,
		},
		{
			name:    "empty dev root",
			mutate:  func(c *Config) { c.DevRoot = "" },
			wantErr: errDevRoot,
		},
	} {
		t.Run(tc.name, func(t *testing.T) {
			cfg, ok := createDefaultConfig().(*Config)
			require.True(t, ok)
			tc.mutate(cfg)
			err := cfg.Validate()
			if tc.wantErr == "" {
				require.NoError(t, err)
				return
			}
			require.Error(t, err)
			assert.Contains(t, err.Error(), tc.wantErr)
		})
	}
}
