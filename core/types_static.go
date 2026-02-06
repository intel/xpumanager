// Copyright (C) 2019-2025 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package core

import "C"
import (
	"unsafe"

	"github.com/intel/level-zero-go/internal"
)

/* Types for the higher level golang API */
type StringProperty64 [64]byte

func (s StringProperty64) String() string {
	return C.GoString((*C.char)(unsafe.Pointer(&s[0])))
}

type StringProperty256 [256]byte

func (s StringProperty256) String() string {
	return C.GoString((*C.char)(unsafe.Pointer(&s[0])))
}

/* Stringers for flags types */

// String representation of all set bits of DevicePropertyFlags.
func (f DevicePropertyFlags) String() string {
	return internal.FlagsToString[DevicePropertyFlag](DevicePropertyFlag(f))
}
