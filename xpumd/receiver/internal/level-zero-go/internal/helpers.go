// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package internal

import (
	"fmt"
	"strings"
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
