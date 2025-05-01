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
#include "enginegroup.h"

enginegroup::~enginegroup()
{
	if (engineGroups)
	{
		delete[] engineGroups;
		engineGroups = nullptr;
	}
}

ze_result_t enginegroup::enumGroups(zes_device_handle_t device)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	result = zesDeviceEnumEngineGroups(device, &engineGroupCount, nullptr);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to enumerate engine groups: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	DBG("Device has %d engine groups.\n", engineGroupCount);

	engineGroups = new zes_engine_handle_t[engineGroupCount];
	result = zesDeviceEnumEngineGroups(device, &engineGroupCount, engineGroups);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get engine group handles: 0x%X (%s)\n", result, l0_error_to_string(result));
	}
	return result;
}

ze_result_t enginegroup::getProperties(zes_engine_handle_t engineGroup)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_engine_properties_t engineProperties = {};
	result = zesEngineGetProperties(engineGroup, &engineProperties);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get engine properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	DBG("  - Engine SType: %d\n", engineProperties.stype);
	DBG("  - Engine Type: %d\n", engineProperties.type);
	DBG("  - Engine Subdevice ID: %d\n", engineProperties.subdeviceId);
	return result;
}

ze_result_t enginegroup::getActivity(zes_engine_handle_t engineGroup)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_engine_stats_t engineStats = {};
	result = zesEngineGetActivity(engineGroup, &engineStats);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get engine activity: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	DBG("  - Timestamp: %" PRIu64 "\n", engineStats.timestamp);
	DBG("  - Active Time: %" PRIu64 "ns\n", engineStats.activeTime);
	return result;
}

ze_result_t enginegroup::getActivityExt(zes_engine_handle_t engineGroup)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_engine_stats_t engineStats = {};
	result = zesEngineGetActivityExt(engineGroup, 0, &engineStats);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get extended engine activity: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	DBG("  - Timestamp: %" PRIu64 "\n", engineStats.timestamp);
	DBG("  - Active Time: %" PRIu64 "ns\n", engineStats.activeTime);
	return result;
}

ze_result_t enginegroup::zesRun(zes_device_handle_t device)
{
	ze_result_t result = enumGroups(device);
	if (result != ZE_RESULT_SUCCESS)
		return result;

	for (uint32_t i = 0; i < engineGroupCount; ++i)
	{
		zes_engine_handle_t engineGroup = engineGroups[i];
		DBG("  - Engine Group handle: %p\n", engineGroup);

		getProperties(engineGroup);
		getActivity(engineGroup);
		getActivityExt(engineGroup);
	}
	return result;
}
