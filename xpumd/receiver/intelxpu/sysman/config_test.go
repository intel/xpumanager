//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestConfigValidate(t *testing.T) {
	tcs := []struct {
		name    string
		setup   func(cfg *Config)
		wantErr string
	}{
		{
			name:  "defaults",
			setup: func(*Config) {},
		},
		{
			name:    "too short sampling interval",
			setup:   func(cfg *Config) { cfg.SamplingInterval = time.Microsecond },
			wantErr: "sampling_interval too short",
		},
		{
			name:    "invalid info log instance name",
			setup:   func(cfg *Config) { cfg.InfoLogs.InstanceName = "instances/xpumd" },
			wantErr: `invalid info_logs.instance_name ("instances/xpumd"), must not contain '/'`,
		},
		{
			name:    "empty info log instance name",
			setup:   func(cfg *Config) { cfg.InfoLogs.InstanceName = "" },
			wantErr: "empty info_logs.instance_name, the name of the collection instance is required",
		},
		{
			name:  "custom info log instance name",
			setup: func(cfg *Config) { cfg.InfoLogs.InstanceName = "xpumd-test" },
		},
	}

	for _, tc := range tcs {
		t.Run(tc.name, func(t *testing.T) {
			cfg, ok := defaultConfig().(*Config)
			require.True(t, ok)
			tc.setup(cfg)

			err := cfg.Validate()
			if tc.wantErr == "" {
				assert.NoError(t, err)
			} else {
				assert.ErrorContains(t, err, tc.wantErr)
			}
		})
	}
}
