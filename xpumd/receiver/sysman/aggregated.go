//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

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
	head        int
	tails       []int
	lostSamples []int
}

// newAggregatedMetric creates a new aggregated metric with buffer size N
func newAggregatedMetric[T numeric](bufferSize, numReaders int) *aggregatedMetric[T] {
	return &aggregatedMetric[T]{
		// Allocate one extra as the sample under head is not available.
		buffer:      make([]T, bufferSize+1),
		tails:       make([]int, numReaders),
		lostSamples: make([]int, numReaders),
	}
}

func (am *aggregatedMetric[T]) size() int { return cap(am.buffer) }

// add adds a sample to the buffer
func (am *aggregatedMetric[T]) add(value T) {
	am.Lock()
	defer am.Unlock()

	// Rely on size being always > 0 (even if the requested buffer size is 0)
	am.buffer[am.head] = value
	am.head = (am.head + 1) % am.size()
	for i := range am.tails {
		// To keep implementation simple, the value under head is considered
		// lost. Makes it easy to track multiple readers without the need of
		// separate counters for valid samples, for example.
		if am.head == am.tails[i] {
			am.tails[i] = (am.tails[i] + 1) % am.size()
			am.lostSamples[i]++
		}
	}
}

// collect returns the min/max/avg of collected samples and resets the reader.
func (am *aggregatedMetric[T]) collect(readerIdx int) aggregatedStats[T] {
	am.Lock()
	defer am.Unlock()

	if readerIdx >= len(am.tails) {
		panic("reader index out of bounds")
	}

	// The sample at the write index (head) is not included in the stats
	stats := am.computeStats(am.tails[readerIdx], am.head)
	stats.lostSamples = am.lostSamples[readerIdx]
	am.reset(readerIdx)
	return stats
}

// computeStats calculates min, max, and average from the buffer.
// Async calls must be done with aggregation lock held.
func (am *aggregatedMetric[T]) computeStats(start, end int) aggregatedStats[T] {
	if start == end {
		return aggregatedStats[T]{}
	}

	minV := am.buffer[start]
	maxV := am.buffer[start]

	var sum float64
	samples := end - start
	if end < start {
		samples += am.size()
	}
	for i := 0; i < samples; i++ {
		idx := (start + i) % am.size()

		val := am.buffer[idx]
		if val < minV {
			minV = val
		}
		if val > maxV {
			maxV = val
		}
		sum += float64(val)
	}

	return aggregatedStats[T]{
		minValue: minV,
		maxValue: maxV,
		avgValue: sum / float64(samples),
		samples:  samples,
	}
}

// reset clears the reader index and counters, resetting the collection window.
// Async calls must be done with aggregation lock held.
func (am *aggregatedMetric[T]) reset(readerIdx int) {
	am.tails[readerIdx] = am.head
	am.lostSamples[readerIdx] = 0
}
