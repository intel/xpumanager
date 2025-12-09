//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestAggregatedMetricsStats(t *testing.T) {
	tests := []struct {
		name       string
		bufferSize int
		values     []int64
		expected   aggregatedStats[int64]
	}{
		{
			name: "Zero-sized buffer",
			// Edge case: buffer size of zero
			bufferSize: 0,
			values:     []int64{1, 2, 3},
			expected:   aggregatedStats[int64]{lostSamples: 3},
		},
		{
			name:       "Empty buffer",
			bufferSize: 5,
			values:     []int64{},
			expected:   aggregatedStats[int64]{},
		},
		{
			name:       "Single value",
			bufferSize: 5,
			values:     []int64{42},
			expected:   aggregatedStats[int64]{minValue: 42, maxValue: 42, avgValue: 42.0, samples: 1, lostSamples: 0},
		},
		{
			name:       "Full buffer",
			bufferSize: 5,
			values:     []int64{10, 20, 30, 40, 50},
			expected:   aggregatedStats[int64]{minValue: 10, maxValue: 50, avgValue: 30.0, samples: 5, lostSamples: 0},
		},
		{
			name:       "Overflow buffer",
			bufferSize: 5,
			values:     []int64{1, 2, 3, 4, 5, 6, 7},
			expected:   aggregatedStats[int64]{minValue: 3, maxValue: 7, avgValue: 5.0, samples: 5, lostSamples: 2},
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			am := newAggregatedMetric[int64](tt.bufferSize)
			for _, v := range tt.values {
				am.add(v)
			}
			stats := am.collect()
			assert.Equal(t, tt.expected, stats)
		})
	}
}

func TestAggregatedMetricsCollect(t *testing.T) {
	/* Test multiple collects */
	am := newAggregatedMetric[int64](3)
	am.add(10)
	am.add(20)
	stats := am.collect()
	expected := aggregatedStats[int64]{minValue: 10, maxValue: 20, avgValue: 15.0, samples: 2, lostSamples: 0}
	require.Equal(t, expected, stats)

	am.add(30)
	am.add(40)
	am.add(50)
	stats = am.collect()
	expected = aggregatedStats[int64]{minValue: 30, maxValue: 50, avgValue: 40.0, samples: 3, lostSamples: 0}
	require.Equal(t, expected, stats)
}
