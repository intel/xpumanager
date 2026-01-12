/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "enginegroup.h"

/**
 * @brief Destructor for the enginegroup class
 *
 * This destructor performs cleanup operations for the engine group management
 * object, releasing allocated memory for engine group handles and ensuring
 * proper resource deallocation when the enginegroup object is destroyed.
 */
enginegroup::~enginegroup()
{
	if (engineGroups) {
		delete[] engineGroups;
		engineGroups = nullptr;
	}
}

/**
 * @brief Enumerates available engine groups for a device
 *
 * This function discovers and catalogs all engine groups available on the
 * specified device. Engine groups represent different types of GPU execution
 * units such as compute, media, copy, and render engines that can be monitored
 * and managed independently.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
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

/**
 * @brief Gets properties for a specific engine group
 *
 * This function retrieves detailed properties and characteristics for a
 * specific engine group, including engine type, subdevice information,
 * and execution unit classification for workload management.
 *
 * @param engineGroup Handle to the specific engine group
 * @param engineProperties Pointer to structure to store engine group properties
 * @return ze_result_t ZE_RESULT_SUCCESS on successful property retrieval, error code otherwise
 */
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

/**
 * @brief Gets activity statistics for a specific engine group
 *
 * This function retrieves current activity metrics for an engine group,
 * including active time and timestamp information for utilization calculations
 * and performance monitoring.
 *
 * @param engineGroup Handle to the specific engine group
 * @param engineStats Pointer to structure to store engine activity statistics
 * @return ze_result_t ZE_RESULT_SUCCESS on successful activity retrieval, error code otherwise
 */
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

/**
 * @brief Gets extended activity statistics for a specific engine group
 *
 * This function retrieves extended activity metrics for an engine group,
 * providing additional performance monitoring capabilities beyond basic
 * activity statistics for advanced workload analysis.
 *
 * @param engineGroup Handle to the specific engine group
 * @return ze_result_t ZE_RESULT_SUCCESS on successful extended activity retrieval, error code otherwise
 */
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

/**
 * @brief Counts the number of engine instances of a specific type
 *
 * This function iterates through all engine groups to count how many
 * engines match the specified engine type.
 *
 * @param [out] count Pointer to variable to store the count of engine instances
 * @param [in] type The specific engine group type to count
 * @return ze_result_t ZE_RESULT_SUCCESS if counting completed successfully, error code otherwise
 */
ze_result_t enginegroup::getEngineCountByType(uint32_t *count, zes_engine_group_t type)
{
	zes_engine_properties_t engineProperties;
	ze_result_t result = ZE_RESULT_SUCCESS;
	TRACING();

	if (count == nullptr) {
		ERR("Count pointer is null.\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}

	*count = 0;

	for (uint32_t i = 0; i < engineGroupCount; ++i) {
		zes_engine_handle_t engineGroup = engineGroups[i];
		result = getProperties(engineGroup, &engineProperties);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get engine properties for group %d: 0x%X (%s)\n", i, result, l0_error_to_string(result));
			return result;
		}

		DBG("  - Engine Group %d Type: %d\n", i, engineProperties.type);
		if (engineProperties.type == type) {
			(*count)++;
		}
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Gets utilization statistics for specific engine group types
 *
 * This function searches for engine groups matching the specified types
 * and retrieves their utilization metrics, providing active time and
 * timestamp data for performance analysis and monitoring.
 *
 * @param typeTable Array of engine group types to search for
 * @param tableSize Number of entries in the type table
 * @param utilization Pointer to store the utilization active time
 * @param timestamp Pointer to store the measurement timestamp
 * @return ze_result_t ZE_RESULT_SUCCESS if utilization retrieved successfully, error code otherwise
 */
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

/**
 * @brief Gets activity statistics for a specific engine instance by type and index
 *
 * This function retrieves utilization activity (active time and timestamp) for
 * a specific engine instance. Engines are indexed in the order they appear when
 * filtered by type.
 *
 * @param [in] type The engine group type to query
 * @param [in] engineIndex Zero-based index of the engine instance within engines of this type
 * @param [out] activeTime Pointer to store the cumulative active time in microseconds
 * @param [out] timestamp Pointer to store the measurement timestamp in microseconds
 * @return ze_result_t ZE_RESULT_SUCCESS if activity retrieved successfully, error code otherwise
 */
ze_result_t enginegroup::getEngineActivityByType(zes_engine_group_t type, uint32_t engineIndex, uint64_t *activeTime,
												 uint64_t *timestamp)
{
	zes_engine_properties_t engineProperties;
	zes_engine_stats_t engineStats;
	ze_result_t result = ZE_RESULT_SUCCESS;
	uint32_t matchIndex = 0;
	TRACING();

	if (activeTime == nullptr) {
		ERR("Active time pointer is null.\n");
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

		if (engineProperties.type == type) {
			if (matchIndex == engineIndex) {
				result = getActivity(engineGroup, &engineStats);
				if (result != ZE_RESULT_SUCCESS) {
					ERR("Failed to get engine activity for group %d: 0x%X (%s)\n", i, result,
						l0_error_to_string(result));
					return result;
				}

				*activeTime = engineStats.activeTime;
				*timestamp = engineStats.timestamp;

				DBG("Engine type %d index %u: activeTime=%" PRIu64 ", timestamp=%" PRIu64 "\n", type, engineIndex,
					*activeTime, *timestamp);
				return ZE_RESULT_SUCCESS;
			}
			matchIndex++;
		}
	}

	ERR("Engine type %d index %u not found (only %u engines of this type)\n", type, engineIndex, matchIndex);
	return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

/**
 * @brief Gets activity statistics per tile for an aggregated engine type
 *
 * This function retrieves activity (active time and timestamp) for engine groups
 * of the specified type, organized by tile (subdevice). Designed for use with
 * *_ALL engine types (e.g., ZES_ENGINE_GROUP_COMPUTE_ALL) where Level Zero provides
 * one aggregated handle per tile. For per-engine instance data, use getEngineActivityByType
 * with *_SINGLE types instead.
 *
 * @param [in] type The aggregated engine group type to query (e.g., ZES_ENGINE_GROUP_COMPUTE_ALL)
 * @param [out] tileActivity Map of tile_id -> (activeTime, timestamp)
 * @return ze_result_t ZE_RESULT_SUCCESS if activity retrieved successfully, error code otherwise
 */
ze_result_t enginegroup::getEngineActivityPerTile(zes_engine_group_t type,
												  std::map<uint32_t, std::pair<uint64_t, uint64_t>> &tileActivity)
{
	zes_engine_properties_t engineProperties;
	zes_engine_stats_t engineStats;
	ze_result_t result = ZE_RESULT_SUCCESS;
	TRACING();

	tileActivity.clear();

	for (uint32_t i = 0; i < engineGroupCount; ++i) {
		zes_engine_handle_t engineGroup = engineGroups[i];
		result = getProperties(engineGroup, &engineProperties);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get engine properties for group %d: 0x%X (%s)\n", i, result, l0_error_to_string(result));
			return result;
		}

		if (engineProperties.type == type) {
			result = getActivity(engineGroup, &engineStats);
			if (result != ZE_RESULT_SUCCESS) {
				ERR("Failed to get engine activity for group %d: 0x%X (%s)\n", i, result, l0_error_to_string(result));
				return result;
			}

			uint32_t tileId = engineProperties.subdeviceId;
			auto &entry = tileActivity[tileId];
			entry.first = engineStats.activeTime;
			entry.second = engineStats.timestamp;

			DBG("Engine type %d tile %u: activeTime=%" PRIu64 ", timestamp=%" PRIu64 "\n", type, tileId,
				engineStats.activeTime, engineStats.timestamp);
		}
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Initializes the engine group management subsystem for a device
 *
 * This function initializes engine group management by enumerating all
 * available engine groups on the device and preparing them for monitoring
 * and utilization tracking operations.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t enginegroup::init(zes_device_handle_t device)
{
	TRACING();
	return enumGroups(device);
}

/**
 * @brief Executes comprehensive engine group monitoring and data collection
 *
 * This function performs complete engine group assessment by collecting
 * properties, activity statistics, and extended metrics for all engine groups,
 * providing comprehensive GPU execution unit utilization monitoring.
 *
 * @param device Handle to the Level Zero Sysman device (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if all engine operations completed successfully, error code otherwise
 */
ze_result_t enginegroup::zesRun(UNUSED zes_device_handle_t device)
{
	zes_engine_properties_t engineProperties;
	zes_engine_stats_t engineStats;
	TRACING();

	for (uint32_t i = 0; i < engineGroupCount; ++i) {
		zes_engine_handle_t engineGroup = engineGroups[i];
		DBG("  - Engine Group handle: %p\n", (void *)engineGroup);

		getProperties(engineGroup, &engineProperties);
		getActivity(engineGroup, &engineStats);
		getActivityExt(engineGroup);
	}
	return ZE_RESULT_SUCCESS;
}
