/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "ras.h"

/**
 * @brief Destructor for the RAS (Reliability, Availability, Serviceability) class
 *
 * This destructor performs cleanup operations for the RAS management object,
 * releasing allocated memory for RAS error set handles and ensuring proper
 * resource deallocation when the RAS object is destroyed.
 */
ras::~ras()
{
	if (rasHandles) {
		delete[] rasHandles;
		rasHandles = nullptr;
	}
}

/**
 * @brief Enumerates available RAS error sets for a device
 *
 * This function discovers and catalogs all RAS error sets available on the
 * specified device. RAS error sets provide hardware reliability monitoring
 * and error detection capabilities for different device components.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
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

/**
 * @brief Gets properties for a specific RAS error set
 *
 * This function retrieves detailed properties and characteristics for a
 * specific RAS error set, including error types, subdevice association,
 * and monitoring capabilities for reliability assessment.
 *
 * @param rasHandle Handle to the specific RAS error set
 * @param properties Pointer to structure to store RAS properties
 * @return ze_result_t ZE_RESULT_SUCCESS on successful property retrieval, error code otherwise
 */
ze_result_t ras::getProperties(zes_ras_handle_t rasHandle, zes_ras_properties_t *properties)
{
	ze_result_t result = zesRasGetProperties(rasHandle, properties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get RAS properties. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("RAS properties retrieved successfully.\n");
	DBG("  onSubdevice: %d\n", properties->onSubdevice);
	DBG("  subdeviceId: %d\n", properties->subdeviceId);

	switch (properties->type) {
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

/**
 * @brief Gets the configuration for a RAS error set
 *
 * This function retrieves the current configuration settings for a specific
 * RAS error set, including error detection thresholds and reporting
 * configuration for reliability monitoring management.
 *
 * @param rasHandle Handle to the specific RAS error set
 * @param config Pointer to structure to store RAS configuration
 * @return ze_result_t ZE_RESULT_SUCCESS on successful configuration retrieval, error code otherwise
 */
ze_result_t ras::getConfig(zes_ras_handle_t rasHandle, zes_ras_config_t *config)
{
	ze_result_t result = zesRasGetConfig(rasHandle, config);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get RAS config. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("RAS config retrieved successfully.\n");
	DBG("  Total Threshold: %" PRIu64 "\n", config->totalThreshold);
	DBG("  Detailed Thresholds:\n");
	for (uint32_t i = 0; i < ZES_MAX_RAS_ERROR_CATEGORY_COUNT; ++i) {
		DBG("    Category %u: %" PRIu64 "\n", i, config->detailedThresholds.category[i]);
	}
	return result;
}

/**
 * @brief Gets the current state for a RAS error set
 *
 * This function retrieves the current error state and statistics for a specific
 * RAS error set, including error counts and categories for reliability
 * monitoring and system health assessment.
 *
 * @param rasHandle Handle to the specific RAS error set
 * @param state Pointer to structure to store RAS state information
 * @return ze_result_t ZE_RESULT_SUCCESS on successful state retrieval, error code otherwise
 */
ze_result_t ras::getState(zes_ras_handle_t rasHandle, zes_ras_state_t *state)
{
	ze_result_t result = zesRasGetState(rasHandle, 0, state);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get RAS state. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("RAS state retrieved successfully.\n");
	for (uint32_t i = 0; i < ZES_MAX_RAS_ERROR_CATEGORY_COUNT; ++i) {
		DBG("    Category %u: %" PRIu64 "\n", i, state->category[i]);
	}

	return result;
}

/**
 * @brief Gets specific RAS error counts by category and type
 *
 * This function retrieves error counts for a specific error category and type
 * across all RAS error sets, providing targeted reliability monitoring for
 * specific hardware components and error conditions.
 *
 * This is a convenience wrapper around getErrorsPerTile() that returns just
 * the total count without per-tile breakdown.
 *
 * @param [in] type The RAS error category to query
 * @param [in] errorType The specific error type (correctable or uncorrectable)
 * @param [out] rasCounter Pointer to store the accumulated error count
 * @return ze_result_t ZE_RESULT_SUCCESS on successful error count retrieval, error code otherwise
 */
ze_result_t ras::getErrors(zes_ras_error_cat_t type, zes_ras_error_type_t errorType, uint64_t *rasCounter)
{
	TRACING();

	if (rasCounter == nullptr) {
		ERR("Invalid argument: rasCounter is null\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}

	// Use getErrorsPerTile and just return the total
	std::map<uint32_t, uint64_t> countersPerTile;
	ze_result_t result = getErrorsPerTile(type, errorType, countersPerTile, rasCounter);

	if (result == ZE_RESULT_SUCCESS) {
		DBG("RAS error count for category %d: %" PRIu64 "\n", type, *rasCounter);
	}

	return result;
}

/**
 * @brief Gets RAS error counts per tile for a specific category and type
 *
 * This function retrieves error counts for each tile/subdevice separately,
 * along with the total count across all tiles. This enables proper multi-tile
 * device monitoring where each tile's RAS errors can be tracked independently.
 *
 * @param [in] type The RAS error category to query
 * @param [in] errorType The specific error type (correctable or uncorrectable)
 * @param [out] countersPerTile Map to store per-tile error counts (tile_id -> count)
 * @param [out] totalCounter Pointer to store the total error count across all tiles
 * @return ze_result_t ZE_RESULT_SUCCESS on successful error count retrieval, error code otherwise
 */
ze_result_t ras::getErrorsPerTile(zes_ras_error_cat_t type, zes_ras_error_type_t errorType,
								  std::map<uint32_t, uint64_t> &countersPerTile, uint64_t *totalCounter)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_ras_properties_t properties = {};
	zes_ras_state_t states = {};

	if (type < ZES_RAS_ERROR_CAT_RESET || type > ZES_RAS_ERROR_CAT_DISPLAY_ERRORS) {
		ERR("Invalid RAS error category type: %d\n", type);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (totalCounter == nullptr) {
		ERR("Invalid argument: totalCounter is null\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}

	countersPerTile.clear();
	*totalCounter = 0;

	for (uint32_t i = 0; i < rasCount; ++i) {
		result = getProperties(rasHandles[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = getState(rasHandles[i], &states);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		// For cache errors, filter by properties.type to get correctable/uncorrectable separately
		// For other categories, sum all handles (legacy behavior)
		if (type == ZES_RAS_ERROR_CAT_CACHE_ERRORS && errorType != ZES_RAS_ERROR_TYPE_FORCE_UINT32 &&
			properties.type != errorType) {
			continue;
		}

		uint64_t errorCount = states.category[type];

		uint32_t tileId = properties.onSubdevice ? properties.subdeviceId : 0;

		countersPerTile[tileId] += errorCount;
		*totalCounter += errorCount;
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Initializes the RAS management module for a device
 *
 * This function performs initial setup of RAS monitoring capabilities by
 * enumerating all available RAS error sets on the specified device for
 * subsequent reliability monitoring and error detection operations.
 *
 * @param device Handle to the device for RAS initialization
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t ras::init(zes_device_handle_t device)
{
	TRACING();
	return enumRasErrorSets(device);
}

/**
 * @brief Performs comprehensive RAS monitoring runtime operations
 *
 * This function executes a complete RAS monitoring cycle including property
 * retrieval, configuration checking, and state monitoring for all RAS error
 * sets to provide comprehensive reliability and serviceability assessment.
 *
 * @param device Handle to the device (unused in current implementation)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful execution, error code otherwise
 */
ze_result_t ras::zesRun(UNUSED zes_device_handle_t device)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_ras_properties_t properties = {};
	zes_ras_config_t config = {};
	zes_ras_state_t state = {};

	for (uint32_t i = 0; i < rasCount; ++i) {
		result = getProperties(rasHandles[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = getConfig(rasHandles[i], &config);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = getState(rasHandles[i], &state);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
	}

	return result;
}