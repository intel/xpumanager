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
#include "ras.h"

ras::~ras()
{
	if (rasHandles) {
		delete[] rasHandles;
		rasHandles = nullptr;
	}
}

ze_result_t ras::enumRasErrorSets(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumRasErrorSets(device, &rasCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || rasCount == 0) {
		ERR("Failed to enumerate RAS error sets. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	rasHandles = new zes_ras_handle_t[rasCount];
	result = zesDeviceEnumRasErrorSets(device, &rasCount, rasHandles);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get RAS error sets. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Found %d RAS domains.\n", rasCount);
	return result;
}

ze_result_t ras::getProperties(zes_ras_handle_t rasHandle)
{
	zes_ras_properties_t properties;
	ze_result_t result = zesRasGetProperties(rasHandle, &properties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get RAS properties. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("RAS properties retrieved successfully.\n");
	DBG("  onSubdevice: %d\n", properties.onSubdevice);
	DBG("  subdeviceId: %d\n", properties.subdeviceId);

	switch (properties.type) {
	case ZES_RAS_ERROR_TYPE_CORRECTABLE:
		DBG("  Type: Correctable\n");
		break;
	case ZES_RAS_ERROR_TYPE_UNCORRECTABLE:
		DBG("  Type: Uncorrectable\n");
		break;
	default:
		DBG("  Type: Unknown\n");
		break;
	}
	return result;
}

ze_result_t ras::getConfig(zes_ras_handle_t rasHandle)
{
	zes_ras_config_t config;
	ze_result_t result = zesRasGetConfig(rasHandle, &config);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get RAS config. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("RAS config retrieved successfully.\n");
	DBG("  Total Threshold: %" PRIu64 "\n", config.totalThreshold);
	DBG("  Detailed Thresholds:\n");
	for (uint32_t i = 0; i < ZES_MAX_RAS_ERROR_CATEGORY_COUNT; ++i) {
		DBG("    Category %u: %" PRIu64 "\n", i, config.detailedThresholds.category[i]);
	}
	return result;
}

ze_result_t ras::getState(zes_ras_handle_t rasHandle)
{
	zes_ras_state_t state = {};
	ze_result_t result = zesRasGetState(rasHandle, 0, &state);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get RAS state. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("RAS state retrieved successfully.\n");
	for (uint32_t i = 0; i < ZES_MAX_RAS_ERROR_CATEGORY_COUNT; ++i) {
		DBG("    Category %u: %" PRIu64 "\n", i, state.category[i]);
	}

	return result;
}

ze_result_t ras::zesRun(zes_device_handle_t device)
{
	ze_result_t result = enumRasErrorSets(device);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	for (uint32_t i = 0; i < rasCount; ++i) {
		result = getProperties(rasHandles[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = getConfig(rasHandles[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = getState(rasHandles[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
	}

	return result;
}