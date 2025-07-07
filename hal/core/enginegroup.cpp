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
	if (engineGroups) {
		delete[] engineGroups;
		engineGroups = nullptr;
	}
}

ze_result_t enginegroup::enumGroups(zes_device_handle_t device)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	TRACING();

	result = zesDeviceEnumEngineGroups(device, &engineGroupCount, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to enumerate engine groups: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	DBG("Device has %d engine groups.\n", engineGroupCount);

	engineGroups = new zes_engine_handle_t[engineGroupCount];
	result = zesDeviceEnumEngineGroups(device, &engineGroupCount, engineGroups);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get engine group handles: 0x%X (%s)\n", result, l0_error_to_string(result));
	}
	return result;
}

ze_result_t enginegroup::getProperties(zes_engine_handle_t engineGroup, zes_engine_properties_t *engineProperties)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	TRACING();

	result = zesEngineGetProperties(engineGroup, engineProperties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get engine properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	DBG("  - Engine SType: %d\n", engineProperties->stype);
	switch (engineProperties->type) {
	case ZES_ENGINE_GROUP_ALL:
		DBG("  - Engine Type: All\n");
		break;
	case ZES_ENGINE_GROUP_COMPUTE_ALL:
		DBG("  - Engine Type: Compute\n");
		break;
	case ZES_ENGINE_GROUP_MEDIA_ALL:
		DBG("  - Engine Type: Media All\n");
		break;
	case ZES_ENGINE_GROUP_COPY_ALL:
		DBG("  - Engine Type: Copy All\n");
		break;
	case ZES_ENGINE_GROUP_COMPUTE_SINGLE:
		DBG("  - Engine Type: Compute Single\n");
		break;
	case ZES_ENGINE_GROUP_RENDER_SINGLE:
		DBG("  - Engine Type: Render Single\n");
		break;
	case ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE:
		DBG("  - Engine Type: Media Decode Single\n");
		break;
	case ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE:
		DBG("  - Engine Type: Media Encode Single\n");
		break;
	case ZES_ENGINE_GROUP_COPY_SINGLE:
		DBG("  - Engine Type: Copy Single\n");
		break;
	case ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE:
		DBG("  - Engine Type: Media Enhancement Single\n");
		break;
	case ZES_ENGINE_GROUP_3D_SINGLE:
		DBG("  - Engine Type: 3D Single\n");
		break;
	case ZES_ENGINE_GROUP_3D_RENDER_COMPUTE_ALL:
		DBG("  - Engine Type: 3D Render Compute All\n");
		break;
	case ZES_ENGINE_GROUP_RENDER_ALL:
		DBG("  - Engine Type: Render All\n");
		break;
	case ZES_ENGINE_GROUP_3D_ALL:
		DBG("  - Engine Type: 3D All\n");
		break;
	case ZES_ENGINE_GROUP_MEDIA_CODEC_SINGLE:
		DBG("  - Engine Type: Media Codec Single\n");
		break;
	default:
		DBG("  - Engine Type: Unknown (%d)\n", engineProperties->type);
		break;
	}

	DBG("  - Engine Subdevice ID: %d\n", engineProperties->subdeviceId);
	return result;
}

ze_result_t enginegroup::getActivity(zes_engine_handle_t engineGroup, zes_engine_stats_t *engineStats)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	TRACING();

	memset(engineStats, 0, sizeof(zes_engine_stats_t));
	result = zesEngineGetActivity(engineGroup, engineStats);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get engine activity: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	DBG("  - Timestamp: %" PRIu64 "\n", engineStats->timestamp);
	DBG("  - Active Time: %" PRIu64 "ns\n", engineStats->activeTime);
	return result;
}

ze_result_t enginegroup::getActivityExt(zes_engine_handle_t engineGroup)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_engine_stats_t engineStats = {};
	TRACING();

	result = zesEngineGetActivityExt(engineGroup, 0, &engineStats);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get extended engine activity: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	DBG("  - Timestamp: %" PRIu64 "\n", engineStats.timestamp);
	DBG("  - Active Time: %" PRIu64 "ns\n", engineStats.activeTime);
	return result;
}

ze_result_t enginegroup::getMediaEngines(uint32_t *mediaEngines, zes_engine_group_t type)
{
	zes_engine_properties_t engineProperties;
	ze_result_t result = ZE_RESULT_SUCCESS;
	TRACING();

	if (mediaEngines == nullptr) {
		ERR("Media engines pointer is null.\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}

	for (uint32_t i = 0; i < engineGroupCount; ++i) {
		zes_engine_handle_t engineGroup = engineGroups[i];
		result = getProperties(engineGroup, &engineProperties);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get engine properties for group %d: 0x%X (%s)\n", i, result, l0_error_to_string(result));
			return result;
		}

		DBG("  - Engine Group %d Type: %d\n", i, engineProperties.type);
		if (engineProperties.type == type) {
			(*mediaEngines)++;
		}
	}
	return ZE_RESULT_SUCCESS;
}

ze_result_t enginegroup::getUtilization(zes_engine_group_t *typeTable, uint32_t tableSize, uint64_t *utilization,
										uint64_t *timestamp)
{
	zes_engine_properties_t engineProperties;
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_engine_stats_t engineStats;
	bool found;
	TRACING();

	if (utilization == nullptr) {
		ERR("Utilization pointer is null.\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}

	if (timestamp == nullptr) {
		ERR("Timestamp pointer is null.\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}

	for (uint32_t i = 0; i < engineGroupCount; ++i) {
		zes_engine_handle_t engineGroup = engineGroups[i];
		result = getProperties(engineGroup, &engineProperties);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get engine properties for group %d: 0x%X (%s)\n", i, result, l0_error_to_string(result));
			return result;
		}

		found = false;
		for (uint32_t j = 0; j < tableSize; j++) {
			if (engineProperties.type == typeTable[j]) {
				found = true;
				break;
			}
		}

		if (found) {
			result = getActivity(engineGroup, &engineStats);
			if (result != ZE_RESULT_SUCCESS) {
				ERR("Failed to get engine activity for group %d: 0x%X (%s)\n", i, result, l0_error_to_string(result));
				return result;
			}
			*utilization = engineStats.activeTime;
			*timestamp = engineStats.timestamp;
			DBG("  - Engine Group %d Utilization: %" PRIu64 "%%, Timestamp: %" PRIu64 "\n", i, *utilization,
				*timestamp);
			break;
		}
	}
	return ZE_RESULT_SUCCESS;
}

ze_result_t enginegroup::init(zes_device_handle_t device)
{
	TRACING();
	return enumGroups(device);
}

ze_result_t enginegroup::zesRun(zes_device_handle_t device)
{
	zes_engine_properties_t engineProperties;
	zes_engine_stats_t engineStats;
	UNUSED(device);
	TRACING();

	for (uint32_t i = 0; i < engineGroupCount; ++i) {
		zes_engine_handle_t engineGroup = engineGroups[i];
		DBG("  - Engine Group handle: %p\n", engineGroup);

		getProperties(engineGroup, &engineProperties);
		getActivity(engineGroup, &engineStats);
		getActivityExt(engineGroup);
	}
	return ZE_RESULT_SUCCESS;
}
