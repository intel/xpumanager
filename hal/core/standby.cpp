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
#include "standby.h"

standby::~standby()
{
	if (standbyHandles) {
		delete[] standbyHandles;
		standbyHandles = nullptr;
	}
}

ze_result_t standby::enumStandbyDomains(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumStandbyDomains(device, &standbyCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || standbyCount == 0) {
		ERR("Failed to enumerate standby domains. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	standbyHandles = new zes_standby_handle_t[standbyCount];
	result = zesDeviceEnumStandbyDomains(device, &standbyCount, standbyHandles);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get standby domains. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Found %d standby domains.\n", standbyCount);
	return result;
}

ze_result_t standby::getProperties(zes_standby_handle_t standbyHandle)
{
	zes_standby_properties_t properties = {};
	ze_result_t result = zesStandbyGetProperties(standbyHandle, &properties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get properties for standby domain 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Standby Properties:\n");
	DBG("  onSubdevice: %d\n", properties.onSubdevice);
	DBG("  subdeviceId: %d\n", properties.subdeviceId);
	DBG("  type: %d\n", properties.type);

	return result;
}

ze_result_t standby::getMode(zes_standby_handle_t standbyHandle)
{
	zes_standby_promo_mode_t mode = {};
	ze_result_t result = zesStandbyGetMode(standbyHandle, &mode);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get state for standby domain 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Standby Mode:\n");
	switch (mode) {
	case ZES_STANDBY_PROMO_MODE_DEFAULT:
		DBG("  Default\n");
		break;
	case ZES_STANDBY_PROMO_MODE_NEVER:
		DBG("  Never\n");
		break;
	default:
		DBG("  Unknown\n");
		break;
	}

	return result;
}

ze_result_t standby::setMode(zes_standby_promo_mode_t mode)
{
	ze_result_t result = ZE_RESULT_SUCCESS;

	if (mode != ZES_STANDBY_PROMO_MODE_DEFAULT && mode != ZES_STANDBY_PROMO_MODE_NEVER) {
		ERR("Invalid standby mode. Valid options are default and never.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	for (uint32_t i = 0; i < standbyCount; ++i) {
		result = zesStandbySetMode(standbyHandles[i], mode);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to set standby mode. 0x%X (%s)\n", result, l0_error_to_string(result));
			return result;
		}
	}
	return result;
}

ze_result_t standby::init(zes_device_handle_t device) { return enumStandbyDomains(device); }

ze_result_t standby::zesRun(UNUSED zes_device_handle_t device)
{
	ze_result_t result = ZE_RESULT_SUCCESS;

	for (uint32_t i = 0; i < standbyCount; ++i) {
		result = getProperties(standbyHandles[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = getMode(standbyHandles[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
	}

	return result;
}