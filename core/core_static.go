// Copyright (C) 2019-2025 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

//go:generate ../hack/generate-stringer.sh

package core

import "errors"

func (r *Result) ToError() error {
	if *r == RESULT_SUCCESS {
		return nil
	}
	return errors.New(r.String())
}
