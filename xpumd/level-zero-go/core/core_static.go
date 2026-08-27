// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

//go:generate ../hack/generate-stringer.sh

package core

/*
#cgo pkg-config: level-zero
#include "ze_api.h"
#include <stdlib.h>
#include "cgo_helpers.h"
*/
import "C"

// Check that the API major version of the C headers matches the version used to
// generate the bindings, and that the minor version is not older.
const (
	_level_zero_header_major_API_version_too_old = int(C.ZE_API_VERSION_CURRENT>>16) - int(API_VERSION_CURRENT>>16)
	_level_zero_header_major_API_version_too_new = int(API_VERSION_CURRENT>>16) - int(C.ZE_API_VERSION_CURRENT>>16)
	_level_zero_header_minor_API_version_too_old = int(C.ZE_API_VERSION_CURRENT&0xffff) - int(API_VERSION_CURRENT&0xffff)
)

var (
	_ [_level_zero_header_major_API_version_too_old]struct{}
	_ [_level_zero_header_major_API_version_too_new]struct{}
	_ [_level_zero_header_minor_API_version_too_old]struct{}
)

func (r Result) Error() string {
	return r.String()
}

// ToError converts the Result to an error.
func (r *Result) ToError() error {
	if *r == RESULT_SUCCESS {
		return nil
	}
	return *r
}
