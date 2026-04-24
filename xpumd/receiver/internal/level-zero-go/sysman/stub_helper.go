//go:build test

// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Intel Corporation

package sysman

/*
#include "sysman_state.h"
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"unsafe"
)

// stubReload resets stub state and loads fresh state from the given YAML file.
func stubReload(configFilePath string) error {
	cs := C.CString(configFilePath)
	defer C.free(unsafe.Pointer(cs))
	C.sysman_state_reset()
	if C.sysman_state_load(cs) != 0 {
		return fmt.Errorf("sysman_reload failed for %q", configFilePath)
	}
	return nil
}
