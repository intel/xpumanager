// Copyright (C) 2019-2025 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package core

import "C"
import "unsafe"

/* Types for the higher level golang API */
type StringProperty64 [64]byte

func (s StringProperty64) String() string {
	return C.GoString((*C.char)(unsafe.Pointer(&s[0])))
}

type StringProperty256 [256]byte

func (s StringProperty256) String() string {
	return C.GoString((*C.char)(unsafe.Pointer(&s[0])))
}
