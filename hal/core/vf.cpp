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
#include <osvf.h>

/**
 * @brief Destructor for the Virtual Function (VF) class
 *
 * This destructor performs cleanup operations for the VF management object,
 * releasing allocated memory for both active and enabled VF handle arrays
 * and ensuring proper resource deallocation when the VF object is destroyed.
 */
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

/**
 * @brief Enumerates all active Virtual Functions for a device
 *
 * This function discovers and catalogs all currently active VFs on the
 * specified device. Active VFs are those that are currently running and
 * accessible for monitoring and management operations in the SR-IOV environment.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
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

/**
 * @brief Enumerates all enabled Virtual Functions for a device
 *
 * This function discovers and catalogs all VFs that are enabled on the
 * specified device. Enabled VFs are those that have been configured and
 * are available for activation in the SR-IOV virtualization framework.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
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

/**
 * @brief Gets capabilities and configuration information for a Virtual Function
 *
 * This function retrieves comprehensive capability information for a specific VF,
 * including VF ID, PCI address details (domain, bus, device, function), and
 * device memory size allocation for virtualization management.
 *
 * @param vfHandle Handle to the specific Virtual Function
 * @return ze_result_t ZE_RESULT_SUCCESS on successful capability retrieval, error code otherwise
 */
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

/**
 * @brief Gets memory utilization statistics for a Virtual Function
 *
 * This function retrieves detailed memory usage information for a specific VF,
 * including memory location (system or device memory) and the amount of
 * memory currently utilized by the VF for performance monitoring.
 *
 * @param vfHandle Handle to the specific Virtual Function
 * @return ze_result_t ZE_RESULT_SUCCESS on successful memory utilization retrieval, error code otherwise
 */
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

/**
 * @brief Gets engine utilization statistics for a Virtual Function
 *
 * This function retrieves comprehensive engine usage information for a specific VF,
 * including engine type classification (compute, render, media, copy engines),
 * active counter values, and sampling counter values for detailed performance analysis.
 *
 * @param vfHandle Handle to the specific Virtual Function
 * @return ze_result_t ZE_RESULT_SUCCESS on successful engine utilization retrieval, error code otherwise
 */
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

/*
 * @brief Creates Virtual Functions (VFs) for a device
 *
 * This function allocates and initializes Virtual Functions for the specified
 * device, enabling SR-IOV capabilities and resource management.
 *
 * @param[in] deviceInfo Pointer to the device information structure
 * @return ze_result_t ZE_RESULT_SUCCESS on successful VF creation, error code otherwise
 */
ze_result_t vf::createVFs(DeviceSriovInfo *deviceInfo)
{
	TRACING();

	/* Call OAL Macro for creating VFs */
	if (CREATEVFS(deviceInfo)) {
		ERR("Failed to create VFs.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Initializes the Virtual Function management module for a device
 *
 * This function performs initial setup of VF monitoring capabilities by
 * enumerating both active and enabled Virtual Functions on the specified
 * device for subsequent VF management and monitoring operations.
 *
 * @param device Handle to the device for VF initialization
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
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

/**
 * @brief Performs comprehensive Virtual Function runtime monitoring operations
 *
 * This function executes a complete VF monitoring cycle for all active VFs,
 * including capability retrieval, memory utilization monitoring, and engine
 * utilization tracking for comprehensive SR-IOV performance analysis.
 *
 * @param device Handle to the device (unused in current implementation)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful execution, error code otherwise
 */
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
