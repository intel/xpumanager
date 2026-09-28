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
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api/experimental/ras.html#zesrasgetstateexp
// and if that is not supported, it wraps (legacy) sysman.Ras.GetState instead.
func (z *Ras) GetStateExp() ([]RasStateExp, error) {
	// experimental API supported by backend?
	ret := core.RESULT_ERROR_UNSUPPORTED_FEATURE
	if z.Device().HasExtension(RAS_GET_STATE_EXP_NAME, uint32(RAS_STATE_EXP_VERSION_CURRENT)) {
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
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api/experimental/ras.html#zesrasclearstateexp
//
// Unlike GetStateExp there is no legacy fallback (the legacy API is only able
// to reset all error counters at once) so core.RESULT_ERROR_UNSUPPORTED_FEATURE
// is returned if the experimental API is not available.
func (z *Ras) ClearStateExp(cat RasErrorCategoryExp) error {
	ret := core.RESULT_ERROR_UNSUPPORTED_FEATURE
	if z.Device().HasExtension(RAS_GET_STATE_EXP_NAME, uint32(RAS_STATE_EXP_VERSION_CURRENT)) {
		ret = zesRasClearStateExp(z.handle, cat)
	}
	return ret.ToError()
}

// EnumEnabledVFExp wraps the (experimental) zesDeviceEnumEnabledVFExp function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api/experimental/device.html#zesdeviceenumenabledvfexp
func (z *Device) EnumEnabledVFExp() ([]*VirtualFunction, error) {
	if !z.HasExtension(VIRTUAL_FUNCTION_MANAGEMENT_EXP_NAME, uint32(VF_MANAGEMENT_EXP_VERSION_1_2)) {
		ret := core.RESULT_ERROR_UNSUPPORTED_FEATURE
		return nil, ret.ToError()
	}
	count := uint32(0)
	if ret := zesDeviceEnumEnabledVFExp(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]vfHandle, count)
	ret := zesDeviceEnumEnabledVFExp(z.handle, &count, handles)
	vfs := make([]*VirtualFunction, len(handles))
	for i, h := range handles {
		vfs[i] = &VirtualFunction{handle: h, device: z}
	}
	return vfs, ret.ToError()
}

// GetCapabilitiesExp wraps the (experimental) zesVFManagementGetVFCapabilitiesExp2 function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api/experimental/vf_management.html#zesvfmanagementgetvfcapabilitiesexp2
func (z *VirtualFunction) GetCapabilitiesExp() (VfExp2Capabilities, error) {
	caps := VfExp2Capabilities{stype: _STRUCTURE_TYPE_VF_EXP2_CAPABILITIES}
	ret := zesVFManagementGetVFCapabilitiesExp2(z.handle, &caps)
	return caps, ret.ToError()
}

// GetMemoryUtilizationExp wraps the (experimental) zesVFManagementGetVFMemoryUtilizationExp2 function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api/experimental/vf_management.html#zesvfmanagementgetvfmemoryutilizationexp2
func (z *VirtualFunction) GetMemoryUtilizationExp() ([]VfUtilMemExp2, error) {
	count := uint32(0)
	if ret := zesVFManagementGetVFMemoryUtilizationExp2(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	util := make([]VfUtilMemExp2, count)
	for i := range util {
		util[i].stype = _STRUCTURE_TYPE_VF_UTIL_MEM_EXP2
	}
	ret := zesVFManagementGetVFMemoryUtilizationExp2(z.handle, &count, util)
	return util, ret.ToError()
}

// GetEngineUtilizationExp wraps the (experimental) zesVFManagementGetVFEngineUtilizationExp2 function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api/experimental/vf_management.html#zesvfmanagementgetvfengineutilizationexp2
func (z *VirtualFunction) GetEngineUtilizationExp() ([]VfUtilEngineExp2, error) {
	count := uint32(0)
	if ret := zesVFManagementGetVFEngineUtilizationExp2(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	util := make([]VfUtilEngineExp2, count)
	for i := range util {
		util[i].stype = _STRUCTURE_TYPE_VF_UTIL_ENGINE_EXP2
	}
	ret := zesVFManagementGetVFEngineUtilizationExp2(z.handle, &count, util)
	return util, ret.ToError()
}
