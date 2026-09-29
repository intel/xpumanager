// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package sysman

import "C"

import (
	"sync"
	"unsafe"
)

// cgoAllocMap stores pointers to C allocated memory for future reference.
type cgoAllocMap struct {
	_ sync.RWMutex
	_ map[unsafe.Pointer]struct{}
}

var cgoAllocsUnknown = new(cgoAllocMap)

type sliceHeader struct {
	Data unsafe.Pointer
	Len  int
	Cap  int
}

// unpackPCharString represents the data from a Go string as a NUL-terminated
// *C.char. An empty str maps to NULL.
//
// NOTE: the returned pointer points to the immutable string data, i.e. it is only valid
// for a const char * argument that the C side does not retain after the call.
func unpackPCharString(str string) (*C.char, *cgoAllocMap) {
	if str == "" {
		return nil, cgoAllocsUnknown
	}
	if str[len(str)-1] != 0 {
		str += "\x00"
	}
	return (*C.char)(unsafe.Pointer(unsafe.StringData(str))), cgoAllocsUnknown
}
