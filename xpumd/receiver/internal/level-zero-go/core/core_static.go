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

// Check that the API version of the C headers used for compilation is at least
// the version that was used to generate the bindings.
var (
	// Fails with "cannot use type" compiler error if the major version from the C
	// headers does not match that from the bindings.
	_ [1]struct{} = [int(C.ZE_API_VERSION_CURRENT>>16) - int(API_VERSION_CURRENT>>16) + 1]struct{}{}
	// Fails with "invalid array length" compiler error if the minor version
	// from the C headers is less than that from the bindings.
	_ [int(C.ZE_API_VERSION_CURRENT&0xffff) - int(API_VERSION_CURRENT&0xffff)]struct{}
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
