/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "standby.h"

/**
 * @brief Destructor for the standby class
 *
 * This destructor performs cleanup operations for the standby management
 * object, releasing allocated memory for standby domain handles and ensuring
 * proper resource deallocation when the standby object is destroyed.
 */
standby::~standby()
{
	if (standbyHandles) {
		delete[] standbyHandles;
		standbyHandles = nullptr;
	}
}

/**
 * @brief Enumerates available standby domains for a device
 *
 * This function discovers and catalogs all standby domains available on the
 * specified device. Standby domains control power management states and
 * promotion policies for different GPU components and subsystems.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
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

/**
 * @brief Gets properties for a specific standby domain
 *
 * This function retrieves detailed properties and characteristics for a
 * specific standby domain, including subdevice association and domain type
 * information for power management configuration.
 *
 * @param standbyHandle Handle to the specific standby domain
 * @return ze_result_t ZE_RESULT_SUCCESS on successful property retrieval, error code otherwise
 */
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

/**
 * @brief Gets the current standby promotion mode for a domain
 *
 * This function retrieves the current standby promotion mode setting for a
 * specific standby domain, indicating whether the domain will automatically
 * enter standby states or remain active for power management control.
 *
 * @param standbyHandle Handle to the specific standby domain
 * @return ze_result_t ZE_RESULT_SUCCESS on successful mode retrieval, error code otherwise
 */
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

/**
 * @brief Sets the standby promotion mode for all standby domains
 *
 * This function configures the standby promotion mode across all standby domains,
 * controlling whether components automatically enter standby states (default) or
 * never enter standby (never) for system-wide power management control.
 *
 * @param mode The standby promotion mode to set (DEFAULT or NEVER)
 * @return ze_result_t ZE_RESULT_SUCCESS if mode set successfully, error code otherwise
 */
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

/**
 * @brief Initializes the standby management module for a device
 *
 * This function performs initial setup of standby power management capabilities
 * by enumerating all available standby domains on the specified device for
 * subsequent power state control operations.
 *
 * @param device Handle to the device for standby initialization
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t standby::init(zes_device_handle_t device)
{
	TRACING();
	return enumStandbyDomains(device);
}

/**
 * @brief Performs comprehensive standby domain runtime operations
 *
 * This function executes a complete standby monitoring cycle including property
 * retrieval and current mode checking for all standby domains to provide
 * comprehensive power management state analysis.
 *
 * @param device Handle to the device (unused in current implementation)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful execution, error code otherwise
 */
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