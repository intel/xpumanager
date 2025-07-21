/*
 * Copyright © 2025 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */
#include "vf.h"

vf::~vf()
{
	if (vfActiveHandles) {
		delete[] vfActiveHandles;
		vfActiveHandles = nullptr;
	}
	if (vfEnabledHandles) {
		delete[] vfEnabledHandles;
		vfEnabledHandles = nullptr;
	}
}

ze_result_t vf::enumActiveVF(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumActiveVFExp(device, &vfActiveCount, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get active VF vfActiveCount. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	if (vfActiveCount == 0) {
		DBG("No active VFs found.\n");
		return ZE_RESULT_SUCCESS;
	}

	vfActiveHandles = new zes_vf_handle_t[vfActiveCount];
	result = zesDeviceEnumActiveVFExp(device, &vfActiveCount, vfActiveHandles);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to enumerate active VFs. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	return result;
}

ze_result_t vf::enumEnabledVF(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumEnabledVFExp(device, &vfEnabledCount, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get enabled VF vfEnabledCount. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	if (vfEnabledCount == 0) {
		DBG("No enabled VFs found.\n");
		return ZE_RESULT_SUCCESS;
	}

	vfEnabledHandles = new zes_vf_handle_t[vfEnabledCount];
	result = zesDeviceEnumEnabledVFExp(device, &vfEnabledCount, vfEnabledHandles);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to enumerate enabled VFs. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	return result;
}

ze_result_t vf::getVFCapabilities(zes_vf_handle_t vfHandle)
{
	zes_vf_exp_capabilities_t capabilities = {};
	ze_result_t result = zesVFManagementGetVFCapabilitiesExp(vfHandle, &capabilities);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get VF capabilities. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Successfully retrieved VF capabilities.\n");
	DBG("  - VF ID: %d\n", capabilities.vfID);
	DBG("  - VF Domain: %d\n", capabilities.address.domain);
	DBG("  - VF Bus: %d\n", capabilities.address.bus);
	DBG("  - VF Device: %d\n", capabilities.address.device);
	DBG("  - VF Function: %d\n", capabilities.address.function);
	DBG("  - VF vfDeviceMemSize: %d bytes\n", capabilities.vfDeviceMemSize);

	return result;
}

ze_result_t vf::getVFMemoryUtilization(zes_vf_handle_t vfHandle)
{
	uint32_t memoryUtilCount = 0;
	zes_vf_util_mem_exp2_t memoryUtil = {};
	ze_result_t result = zesVFManagementGetVFMemoryUtilizationExp2(vfHandle, &memoryUtilCount, &memoryUtil);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get VF memory utilization. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Successfully retrieved VF memory utilization.\n");

	switch (memoryUtil.vfMemLocation) {
	case ZES_MEM_LOC_SYSTEM:
		DBG("  - VF Memory Location: System\n");
		break;
	case ZES_MEM_LOC_DEVICE:
		DBG("  - VF Memory Location: Device\n");
		break;
	default:
		DBG("  - VF Memory Location: Unknown\n");
		break;
	}

	DBG("  - VF Memory Utilized: %" PRIu64 "bytes\n", memoryUtil.vfMemUtilized);

	return result;
}

ze_result_t vf::getVFEngineUtilization(zes_vf_handle_t vfHandle)
{
	uint32_t engineUtilCount = 0;
	zes_vf_util_engine_exp2_t engineUtil = {};
	ze_result_t result = zesVFManagementGetVFEngineUtilizationExp2(vfHandle, &engineUtilCount, &engineUtil);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get VF engine utilization. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Successfully retrieved VF engine utilization.\n");

	switch (engineUtil.vfEngineType) {
	case ZES_ENGINE_GROUP_ALL:
		DBG("  - VF Engine Type: All\n");
		break;
	case ZES_ENGINE_GROUP_COMPUTE_ALL:
		DBG("  - VF Engine Type: Compute All\n");
		break;
	case ZES_ENGINE_GROUP_MEDIA_ALL:
		DBG("  - VF Engine Type: Media All\n");
		break;
	case ZES_ENGINE_GROUP_COPY_ALL:
		DBG("  - VF Engine Type: Copy All\n");
		break;
	case ZES_ENGINE_GROUP_COMPUTE_SINGLE:
		DBG("  - VF Engine Type: Compute\n");
		break;
	case ZES_ENGINE_GROUP_RENDER_SINGLE:
		DBG("  - VF Engine Type: Render\n");
		break;
	case ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE:
		DBG("  - VF Engine Type: Media Decode\n");
		break;
	case ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE:
		DBG("  - VF Engine Type: Media Encode\n");
		break;
	case ZES_ENGINE_GROUP_COPY_SINGLE:
		DBG("  - VF Engine Type: Copy\n");
		break;
	case ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE:
		DBG("  - VF Engine Type: Media Enhancement\n");
		break;
	case ZES_ENGINE_GROUP_3D_SINGLE:
		DBG("  - VF Engine Type: 3D\n");
		break;
	case ZES_ENGINE_GROUP_3D_RENDER_COMPUTE_ALL:
		DBG("  - VF Engine Type: 3D Render Compute All\n");
		break;
	case ZES_ENGINE_GROUP_RENDER_ALL:
		DBG("  - VF Engine Type: Render All\n");
		break;
	case ZES_ENGINE_GROUP_3D_ALL:
		DBG("  - VF Engine Type: 3D All\n");
		break;
	case ZES_ENGINE_GROUP_MEDIA_CODEC_SINGLE:
		DBG("  - VF Engine Type: Media Codec\n");
		break;
	default:
		DBG("  - VF Engine Type: Unknown\n");
		break;
	}

	DBG("  - VF Active counter value: %" PRIu64 "\n", engineUtil.activeCounterValue);
	DBG("  - VF Sampling counter value: %" PRIu64 "\n", engineUtil.samplingCounterValue);

	return result;
}

ze_result_t vf::init(zes_device_handle_t device)
{
	ze_result_t result = enumActiveVF(device);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	result = enumEnabledVF(device);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	DBG("Successfully initialized VF.\n");
	return result;
}

ze_result_t vf::zesRun(UNUSED zes_device_handle_t device)
{
	for (uint32_t i = 0; i < vfActiveCount; i++) {
		zes_vf_handle_t v = vfActiveHandles[i];
		getVFCapabilities(v);
		getVFMemoryUtilization(v);
		getVFEngineUtilization(v);
	}
	return ZE_RESULT_SUCCESS;
}