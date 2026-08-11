//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpu

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.opentelemetry.io/collector/confmap"
)

func TestConfigValidate(t *testing.T) {
	tests := []struct {
		name               string
		collectionInterval time.Duration
		samplingInterval   time.Duration
		expectErr          string
	}{
		{
			name:               "default intervals",
			collectionInterval: 1 * time.Minute,
			samplingInterval:   1 * time.Second,
		},
		{
			name:               "minimum intervals",
			collectionInterval: 1 * time.Second,
			samplingInterval:   1 * time.Millisecond,
		},
		{
			name:               "zero sampling_interval",
			collectionInterval: 1 * time.Minute,
			samplingInterval:   0,
			expectErr:          "sampling_interval too short",
		},
		{
			name:               "sampling_interval below one millisecond",
			collectionInterval: 1 * time.Minute,
			samplingInterval:   500 * time.Microsecond,
			expectErr:          "sampling_interval too short",
		},
		{
			name:               "zero collection_interval",
			collectionInterval: 0,
			samplingInterval:   1 * time.Second,
			expectErr:          `"collection_interval"`,
		},
		{
			name:               "collection_interval below one second",
			collectionInterval: 500 * time.Millisecond,
			samplingInterval:   1 * time.Millisecond,
			expectErr:          "collection_interval too short",
		},
		{
			name:               "collection_interval less than twice sampling_interval",
			collectionInterval: 1 * time.Second,
			samplingInterval:   1 * time.Second,
			expectErr:          "must be at least twice the sampling_interval",
		},
		{
			name:               "too many samples per collection cycle",
			collectionInterval: 200 * time.Second,
			samplingInterval:   1 * time.Millisecond,
			expectErr:          "too many samples per collection cycle",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			cfg, ok := defaultConfig().(*Config)
			require.True(t, ok)
			cfg.CollectionInterval = tt.collectionInterval
			cfg.SamplingInterval = tt.samplingInterval

			// Validate through confmap as the collector service does, i.e.
			// including recursive validation of the embedded configs.
			err := confmap.Validate(cfg)

			if tt.expectErr == "" {
				assert.NoError(t, err)
			} else {
				assert.ErrorContains(t, err, tt.expectErr)
			}
		})
	}
}
