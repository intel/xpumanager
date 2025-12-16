// Copyright (C) 2019-2025 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

//go:generate ../hack/generate-stringer.sh

package core

import "errors"

/*
#cgo pkg-config: level-zero
#include "ze_api.h"
#include <stdlib.h>
#include "cgo_helpers.h"
*/
import "C"

// Check that the API version of the C headers used for compilation is at least
// the version that was used to generate the bindings.
// Will fail with "invalid array length" compiler error if the
// ZE_API_VERSION_CURRENT from the C headers is less than API_VERSION_CURRENT
// from the bindings.
var _ [C.ZE_API_VERSION_CURRENT - API_VERSION_CURRENT]struct{}

// ToError converts the Result to an error.
func (r *Result) ToError() error {
	if *r == RESULT_SUCCESS {
		return nil
	}
	return errors.New(r.String())
}
