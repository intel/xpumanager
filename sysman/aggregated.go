//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"sync"
)

type numeric interface {
	~int32 | ~int64 | ~uint32 | ~uint64 | ~float32 | ~float64
}

// aggregatedStats holds the aggregated statistics
type aggregatedStats[T numeric] struct {
	minValue    T
	maxValue    T
	avgValue    float64
	samples     int
	lostSamples int
}

// aggregatedMetric collects samples in a buffer and computes statistics
type aggregatedMetric[T numeric] struct {
	sync.Mutex

	buffer      []T
	size        int
	index       int
	samples     int
	lostSamples int
}

// newAggregatedMetric creates a new aggregated metric with buffer size N
func newAggregatedMetric[T numeric](bufferSize int) *aggregatedMetric[T] {
	return &aggregatedMetric[T]{
		buffer: make([]T, bufferSize),
		size:   bufferSize,
	}
}

// add adds a sample to the buffer
func (am *aggregatedMetric[T]) add(value T) {
	am.Lock()
	defer am.Unlock()

	if am.samples < am.size {
		am.samples++
	} else {
		am.lostSamples++
	}

	if am.size > 0 {
		am.buffer[am.index] = value
		am.index = (am.index + 1) % am.size
	}
}

// collect returns the min/max/avg of collected samples and resets the buffer.
func (am *aggregatedMetric[T]) collect() aggregatedStats[T] {
	am.Lock()
	defer am.Unlock()

	stats := am.computeStats()
	am.reset()
	return stats
}

// computeStats calculates min, max, and average from the buffer.
func (am *aggregatedMetric[T]) computeStats() aggregatedStats[T] {
	if am.samples == 0 {
		return aggregatedStats[T]{lostSamples: am.lostSamples}
	}

	minV := am.buffer[0]
	maxV := am.buffer[0]

	var sum float64
	for i := 0; i < am.samples; i++ {
		val := am.buffer[i]
		if val < minV {
			minV = val
		}
		if val > maxV {
			maxV = val
		}
		sum += float64(val)
	}

	return aggregatedStats[T]{
		minValue:    minV,
		maxValue:    maxV,
		avgValue:    sum / float64(am.samples),
		samples:     am.samples,
		lostSamples: am.lostSamples,
	}
}

// reset clears the buffer counters, resetting the collection window.
func (am *aggregatedMetric[T]) reset() {
	am.index = 0
	am.samples = 0
	am.lostSamples = 0
}
