//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelcrashlog

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
)

func TestConfigValidate(t *testing.T) {
	tests := []struct {
		name      string
		config    Config
		expectErr string
	}{
		{
			name:   "minimal config",
			config: Config{Directory: "/var/log/crashlog", Glob: "*.bin"},
		},
		{
			name: "all fields set",
			config: Config{
				Directory:       "/var/log/crashlog",
				Glob:            "dev-*-crashlog.bin",
				IgnoreOlderThan: time.Hour,
				AddAttributes:   map[string]string{"hw.vendor": "ACME"},
			},
		},
		{
			name:      "empty directory",
			config:    Config{Glob: "*.bin"},
			expectErr: "directory must not be empty",
		},
		{
			name:      "empty glob",
			config:    Config{Directory: "/var/log/crashlog"},
			expectErr: "glob must not be empty",
		},
		{
			name:      "malformed glob",
			config:    Config{Directory: "/var/log/crashlog", Glob: "[a-"},
			expectErr: "glob is not a valid pattern",
		},
		{
			name:      "negative ignore_older_than",
			config:    Config{Directory: "/var/log/crashlog", Glob: "*.bin", IgnoreOlderThan: -time.Second},
			expectErr: "ignore_older_than must not be negative",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := tt.config.Validate()

			if tt.expectErr == "" {
				assert.NoError(t, err)
			} else {
				assert.ErrorContains(t, err, tt.expectErr)
			}
		})
	}
}
