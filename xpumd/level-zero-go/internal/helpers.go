// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package internal

import (
	"fmt"
	"math"
	"strings"
	"time"
)

type flagType interface {
	~uint32
	fmt.Stringer
}

func FlagsToString[T flagType](flags T) string {
	var vals []string

	for flags != 0 {
		// Get the lowest set bit
		bit := flags & -flags

		vals = append(vals, T(bit).String())

		// Clear the lowest set bit
		flags &= flags - 1
	}
	return strings.Join(vals, " | ")
}

// FlagsToBits returns a slice of individual set bits from a bitmask flags
// value, starting from the LSB.
func FlagsToBits[T ~uint | ~uint8 | ~uint16 | ~uint32 | ~uint64](flags T) []T {
	var result []T
	for ; flags != 0; flags &= flags - 1 {
		result = append(result, T(flags&-flags)) // get the least significant set bit
	}
	return result
}

// DurationToMillisecondsUint32 converts a time.Duration to milliseconds (uint32).
// Negative durations are treated as infinite timeout (UINT32_MAX).
// Durations that exceed UINT32_MAX-1 milliseconds are clamped to UINT32_MAX-1.
func DurationToMillisecondsUint32(d time.Duration) uint32 {
	if d < 0 {
		return math.MaxUint32
	}

	ms := d.Milliseconds()

	if ms >= math.MaxUint32 {
		return math.MaxUint32 - 1
	}

	return uint32(ms)
}

// DurationToMillisecondsUint64 converts a time.Duration to milliseconds (uint64).
// Negative durations are treated as infinite timeout (UINT64_MAX).
func DurationToMillisecondsUint64(d time.Duration) uint64 {
	if d < 0 {
		return math.MaxUint64
	}
	return uint64(d.Milliseconds())
}
