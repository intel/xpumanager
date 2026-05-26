// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package core

import "C"
import (
	"encoding/json"
	"unsafe"

	"github.com/intel/level-zero-go/internal"
)

/* Types for the higher level golang API */
type StringProperty64 [64]byte

func (s StringProperty64) String() string {
	return C.GoString((*C.char)(unsafe.Pointer(&s[0])))
}

func (s StringProperty64) MarshalJSON() ([]byte, error) {
	return json.Marshal(s.String())
}

func (s StringProperty64) MarshalYAML() (any, error) {
	return s.String(), nil
}

type StringProperty256 [256]byte

func (s StringProperty256) String() string {
	return C.GoString((*C.char)(unsafe.Pointer(&s[0])))
}

func (s StringProperty256) MarshalJSON() ([]byte, error) {
	return json.Marshal(s.String())
}

func (s StringProperty256) MarshalYAML() (any, error) {
	return s.String(), nil
}

/* Stringers for flags types */

// String representation of all set bits of DevicePropertyFlags.
func (f DevicePropertyFlags) String() string {
	return internal.FlagsToString[DevicePropertyFlag](DevicePropertyFlag(f))
}
