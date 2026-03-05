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
	vals := []string{}

	for flags != 0 {
		// Get the lowest set bit
		bit := flags & -flags

		vals = append(vals, T(bit).String())

		// Clear the lowest set bit
		flags &= flags - 1
	}
	return strings.Join(vals, " | ")
}
