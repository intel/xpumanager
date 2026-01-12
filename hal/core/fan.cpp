/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "fan.h"

/**
 * @brief Destructor for the fan class
 *
 * This destructor performs cleanup operations for the fan management
 * object, releasing allocated memory for fan handles and ensuring
 * proper resource deallocation when the fan object is destroyed.
 */
fan::~fan()
{
	if (fanHandles) {
		delete[] fanHandles;
		fanHandles = nullptr;
	}
}

/**
 * @brief Enumerates available fan controllers for a device
 *
 * This function discovers and catalogs all fan controllers available on the
 * specified device. Fan controllers manage cooling fans and provide thermal
 * management capabilities for the GPU device.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
ze_result_t fan::enumFans(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumFans(device, &fanCount, nullptr);
	// Get the count of fans
	if (result != ZE_RESULT_SUCCESS || fanCount == 0) {
		ERR("Failed to enumerate fans or no fans found.\n");
		return result;
	}

	fanHandles = new zes_fan_handle_t[fanCount];

	// Retrieve the fan handles
	if (zesDeviceEnumFans(device, &fanCount, fanHandles) != ZE_RESULT_SUCCESS) {
		ERR("Failed to retrieve fan handles.\n");
		return result;
	}

	return result;
}

/**
 * @brief Prints supported fan speed control modes
 *
 * This utility function decodes and displays the supported fan speed control
 * modes in a human-readable format, including default, fixed, and table modes.
 *
 * @param mode Bitfield containing supported fan speed mode flags
 */
void fan::printSupportedModes(const uint32_t mode)
{
	DBG("    - Supported Modes: %d\n", mode);
	DBG("      - ");
	if (mode & ZES_FAN_SPEED_MODE_DEFAULT)
		DBG("Default ");
	if (mode & ZES_FAN_SPEED_MODE_FIXED)
		DBG("Fixed ");
	if (mode & ZES_FAN_SPEED_MODE_TABLE)
		DBG("Table ");
	DBG("\n");
}

/**
 * @brief Gets properties for a specific fan controller
 *
 * This function retrieves detailed properties and capabilities for a
 * specific fan controller, including maximum RPM, control capabilities,
 * and supported speed control modes.
 *
 * @param fanHandle Handle to the specific fan controller
 * @return ze_result_t ZE_RESULT_SUCCESS on successful property retrieval, error code otherwise
 */
ze_result_t fan::getProperties(zes_fan_handle_t fanHandle)
{
	zes_fan_properties_t properties = {};
	ze_result_t result = zesFanGetProperties(fanHandle, &properties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get fan properties.\n");
		return result;
	}
	DBG("  - Fan Properties:\n");
	DBG("    - Fan SType: %d\n", properties.stype);
	DBG("    - Fan onSubdevice: %d\n", properties.onSubdevice);
	DBG("    - Fan Can Control: %d\n", properties.canControl);
	DBG("    - Fan Subdevice ID: %d\n", properties.subdeviceId);
	DBG("    - Fan Max Speed: %d RPM\n", properties.maxRPM);
	DBG("    - Fan Max Points: %d\n", properties.maxPoints);
	printSupportedModes(properties.supportedModes);

	return result;
}

/**
 * @brief Gets the current configuration for a specific fan controller
 *
 * This function retrieves the current fan configuration including speed mode,
 * fixed speed settings, or speed table configurations for thermal management.
 *
 * @param fanHandle Handle to the specific fan controller
 * @return ze_result_t ZE_RESULT_SUCCESS on successful configuration retrieval, error code otherwise
 */
ze_result_t fan::getConfig(zes_fan_handle_t fanHandle)
{
	zes_fan_config_t config = {};
	ze_result_t result = zesFanGetConfig(fanHandle, &config);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get fan configuration.\n");
		return result;
	}
	DBG("  - Fan Configuration:\n");
	DBG("    - Fan Speed Mode: %d\n", config.mode);
	if (config.mode == ZES_FAN_SPEED_MODE_FIXED) {
		DBG("    - Fixed Speed (RPM): %d %s\n", config.speedFixed.speed,
			config.speedFixed.units == ZES_FAN_SPEED_UNITS_PERCENT ? "%" : "RPM");
	} else if (config.mode == ZES_FAN_SPEED_MODE_TABLE) {
		DBG("    - Table Points Count: %d\n", config.speedTable.numPoints);
		for (int32_t i = 0; i < config.speedTable.numPoints; i++) {
			DBG("      - Point %d: Temperature: %d, Speed: %d %s\n", i, config.speedTable.table[i].temperature,
				config.speedTable.table[i].speed.speed,
				config.speedTable.table[i].speed.units == ZES_FAN_SPEED_UNITS_PERCENT ? "%" : "RPM");
		}
	}

	return result;
}

/**
 * @brief Initializes the fan management subsystem for a device
 *
 * This function initializes fan controller management by enumerating all
 * available fan controllers on the device and preparing them for monitoring
 * and configuration operations.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t fan::init(zes_device_handle_t device) { return enumFans(device); }

/**
 * @brief Executes comprehensive fan controller monitoring and data collection
 *
 * This function performs complete fan controller assessment by collecting
 * properties and configuration information for all fan controllers,
 * providing comprehensive thermal management monitoring.
 *
 * @param device Handle to the Level Zero Sysman device (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if all fan operations completed successfully, error code otherwise
 */
ze_result_t fan::zesRun(UNUSED zes_device_handle_t device)
{
	ze_result_t result = ZE_RESULT_SUCCESS;

	for (uint32_t i = 0; i < fanCount; i++) {
		result = getProperties(fanHandles[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = getConfig(fanHandles[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
	}

	return result;
}