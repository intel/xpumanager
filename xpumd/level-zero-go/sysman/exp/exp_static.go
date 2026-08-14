// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package exp

//go:generate ../../hack/generate-stringer.sh

import (
	"github.com/intel/level-zero-go/core"
)

// GetStateExp wraps the (experimental) zesRasGetStateExp function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesrasgetstateexp
// and if that is not supported, it wraps (legacy) sysman.Ras.GetState instead.
func (z *Ras) GetStateExp() ([]RasStateExp, error) {
	// experimental API supported by backend?
	ret := core.RESULT_ERROR_UNSUPPORTED_FEATURE
	if z.Device().HasExtension(RAS_GET_STATE_EXP_NAME) {
		count := uint32(0)
		if ret = zesRasGetStateExp(z.handle, &count, nil); ret == core.RESULT_SUCCESS {
			states := make([]RasStateExp, count)
			if ret = zesRasGetStateExp(z.handle, &count, states); ret == core.RESULT_SUCCESS {
				return states, ret.ToError()
			}
		}
	}
	if ret != core.RESULT_ERROR_UNSUPPORTED_FEATURE {
		return nil, ret.ToError()
	}

	// no => convert legacy API result instead
	state, err := z.GetState(false)
	if err != nil {
		return nil, err
	}
	states := make([]RasStateExp, len(state.Category))
	for cat, value := range state.Category {
		states[cat] = RasStateExp{
			// RasErrorCategoryExp is superset of RasErrorCat
			Category:     RasErrorCategoryExp(cat),
			ErrorCounter: value,
		}
	}
	return states, nil
}

// ClearStateExp wraps the (experimental) zesRasClearStateExp function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesrasclearstateexp
//
// Unlike GetStateExp there is no legacy fallback (the legacy API is only able
// to reset all error counters at once) so core.RESULT_ERROR_UNSUPPORTED_FEATURE
// is returned if the experimental API is not available.
func (z *Ras) ClearStateExp(cat RasErrorCategoryExp) error {
	ret := core.RESULT_ERROR_UNSUPPORTED_FEATURE
	if z.Device().HasExtension(RAS_GET_STATE_EXP_NAME) {
		ret = zesRasClearStateExp(z.handle, cat)
	}
	return ret.ToError()
}
