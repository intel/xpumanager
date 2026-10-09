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
			config: Config{Directory: "/var/log/crashlog", Globs: []string{"*.bin"}},
		},
		{
			name: "all fields set",
			config: Config{
				Directory:       "/var/log/crashlog",
				Globs:           []string{"dev-*-crashlog.bin", "*.crashlog"},
				IgnoreOlderThan: time.Hour,
				AddAttributes:   map[string]string{"hw.vendor": "ACME"},
			},
		},
		{
			name:      "empty directory",
			config:    Config{Globs: []string{"*.bin"}},
			expectErr: "directory must not be empty",
		},
		{
			name:      "no globs",
			config:    Config{Directory: "/var/log/crashlog"},
			expectErr: "globs must not be empty",
		},
		{
			name:      "empty glob",
			config:    Config{Directory: "/var/log/crashlog", Globs: []string{"*.bin", ""}},
			expectErr: "globs must not contain empty patterns",
		},
		{
			name:      "malformed glob",
			config:    Config{Directory: "/var/log/crashlog", Globs: []string{"*.bin", "[a-"}},
			expectErr: `glob "[a-" is not a valid pattern`,
		},
		{
			name:      "negative ignore_older_than",
			config:    Config{Directory: "/var/log/crashlog", Globs: []string{"*.bin"}, IgnoreOlderThan: -time.Second},
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
