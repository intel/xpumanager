// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package sysman

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
