/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "standby.h"
#include "driver_util.h"
#include <vector>

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
		ERR("Failed to enumerate standby domains. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	standbyHandles = new zes_standby_handle_t[standbyCount];
	result = zesDeviceEnumStandbyDomains(device, &standbyCount, standbyHandles);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get standby domains. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Found {} standby domains.\n", standbyCount);
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
		ERR("Failed to get properties for standby domain 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Standby Properties:\n");
	DBG("  onSubdevice: {}\n", properties.onSubdevice);
	DBG("  subdeviceId: {}\n", properties.subdeviceId);
	DBG("  type: {}\n", properties.type);

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
		ERR("Failed to get state for standby domain 0x{:X} ({})\n", result, l0_error_to_string(result));
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
 * @brief Gets properties for a specific standby domain
 *
 * This function retrieves the properties of a specific standby domain
 * identified by its handle, returning information such as the subdevice
 * association and standby domain type.
 *
 * @param standbyHandle Handle to the specific standby domain
 * @param props Pointer to the standby properties structure to populate
 * @return ze_result_t ZE_RESULT_SUCCESS on successful retrieval, error code otherwise
 */
ze_result_t standby::getProperties(zes_standby_handle_t standbyHandle, zes_standby_properties_t *props)
{
	ze_result_t result = zesStandbyGetProperties(standbyHandle, props);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get properties for standby domain 0x{:X} ({})\n", result, l0_error_to_string(result));
	}
	return result;
}

/**
 * @brief Gets the current standby promotion mode for a specific domain
 *
 * This function retrieves the current standby promotion mode setting for a
 * specific standby domain identified by its handle, indicating whether the
 * domain will automatically enter standby states or remain active.
 *
 * @param standbyHandle Handle to the specific standby domain
 * @param mode Pointer to the standby promotion mode value to populate
 * @return ze_result_t ZE_RESULT_SUCCESS on successful retrieval, error code otherwise
 */
ze_result_t standby::getMode(zes_standby_handle_t standbyHandle, zes_standby_promo_mode_t *mode)
{
	ze_result_t result = zesStandbyGetMode(standbyHandle, mode);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get mode for standby domain 0x{:X} ({})\n", result, l0_error_to_string(result));
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
			ERR("Failed to set standby mode. 0x{:X} ({})\n", result, l0_error_to_string(result));
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
/**
 * @brief Gets the promotion mode of the standby domain that describes the device as a whole.
 *
 * A multi-tile device lists a standby domain for each tile as well as its own; the device's own
 * is the one reported. A device that lists only per-tile domains is described by the first.
 *
 * @param[out] mode Set to the promotion mode.
 *
 * @retval ZE_RESULT_SUCCESS                   The mode was read.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE The device exposes no standby domain.
 * @retval Other                               The error from reading the mode.
 */
ze_result_t standby::getDeviceMode(zes_standby_promo_mode_t &mode)
{
	std::vector<ze_bool_t> onSubdevice;
	onSubdevice.reserve(standbyCount);
	for (uint32_t i = 0; i < standbyCount; ++i) {
		zes_standby_properties_t props{};
		props.stype = ZES_STRUCTURE_TYPE_STANDBY_PROPERTIES;
		// A domain whose properties cannot be read is counted as a sub-device one, so it is chosen last.
		onSubdevice.push_back(getProperties(standbyHandles[i], &props) == ZE_RESULT_SUCCESS ? props.onSubdevice
																							: ze_bool_t{1});
	}
	const auto index = xpum::detail::deviceLevelIndex(onSubdevice);
	if (!index) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	return getMode(standbyHandles[*index], &mode);
}
