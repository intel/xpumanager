/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "standby.h"

namespace {

// Name of a standby promotion mode for debug output
const char *promoModeName(zes_standby_promo_mode_t mode)
{
	switch (mode) {
	case ZES_STANDBY_PROMO_MODE_DEFAULT:
		return "default";
	case ZES_STANDBY_PROMO_MODE_NEVER:
		return "never";
	default:
		return "unknown";
	}
}

} // namespace

/**
 * @brief Destructor for the standby class
 *
 * This destructor performs cleanup operations for the standby management
 * object, restoring any standby mode that preventStandby() overrode, releasing allocated
 * memory for standby domain handles and ensuring proper resource deallocation
 * when the standby object is destroyed.
 */
standby::~standby()
{
	restoreMode();
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

	// An explicitly requested mode must outlive this process, so each domain that takes it
	// is dropped from restoreMode(); a domain left untouched by a mid-loop failure is still restored.
	for (uint32_t i = 0; i < standbyCount; ++i) {
		result = zesStandbySetMode(standbyHandles[i], mode);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to set standby mode. 0x{:X} ({})\n", result, l0_error_to_string(result));
			return result;
		}
		if (i < savedModes.size()) {
			savedModes[i] = ZES_STANDBY_PROMO_MODE_NEVER;
		}
	}

	restorePending = false;
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
 * @brief Keeps every standby domain awake for the rest of this process
 *
 * Sets the standby promotion mode of every domain to ZES_STANDBY_PROMO_MODE_NEVER
 * so the device stays out of standby while telemetry is sampled. The previous mode
 * of each domain is saved so restoreMode() can put it back on exit. Only the first
 * call has any effect; later calls return immediately.
 *
 * Setting the mode may fail (e.g. insufficient permissions); such domains are left as
 * they are and the failure is only logged.
 */
void standby::preventStandby()
{
	if (preventApplied) {
		return;
	}
	preventApplied = true;

	// A domain whose mode can't be read, is already NEVER, or can't be changed is left as
	// NEVER in savedModes so restoreMode() skips it.
	savedModes.assign(standbyCount, ZES_STANDBY_PROMO_MODE_NEVER);
	for (uint32_t i = 0; i < standbyCount; ++i) {
		ze_result_t getResult = zesStandbyGetMode(standbyHandles[i], &savedModes[i]);
		if (getResult != ZE_RESULT_SUCCESS) {
			DBG("Failed to get standby mode for domain {}. 0x{:X} ({})\n", i, getResult, l0_error_to_string(getResult));
			savedModes[i] = ZES_STANDBY_PROMO_MODE_NEVER;
			continue;
		}
		if (savedModes[i] == ZES_STANDBY_PROMO_MODE_NEVER) {
			DBG("Standby mode for domain {} is already never before telemetry sampling; leaving it.\n", i);
			continue;
		}

		ze_result_t setResult = zesStandbySetMode(standbyHandles[i], ZES_STANDBY_PROMO_MODE_NEVER);
		if (setResult != ZE_RESULT_SUCCESS) {
			DBG("Failed to set standby mode to never for domain {}. 0x{:X} ({})\n", i, setResult,
				l0_error_to_string(setResult));
			savedModes[i] = ZES_STANDBY_PROMO_MODE_NEVER;
			continue;
		}
		DBG("Set standby mode for domain {} from {} to never before telemetry sampling.\n", i,
			promoModeName(savedModes[i]));
		restorePending = true;
	}
}

/**
 * @brief Restores the standby modes that preventStandby() overrode
 *
 * Puts every standby domain whose mode preventStandby() changed back to the mode
 * it had before. Does nothing if preventStandby() changed no domain, or if
 * setMode() has since applied an explicitly requested mode.
 */
void standby::restoreMode()
{
	if (!restorePending) {
		return;
	}
	restorePending = false;

	for (uint32_t i = 0; i < standbyCount && i < savedModes.size(); ++i) {
		if (savedModes[i] == ZES_STANDBY_PROMO_MODE_NEVER) {
			continue;
		}
		ze_result_t result = zesStandbySetMode(standbyHandles[i], savedModes[i]);
		if (result != ZE_RESULT_SUCCESS) {
			DBG("Failed to restore standby mode for domain {} to {}. 0x{:X} ({})\n", i, promoModeName(savedModes[i]),
				result, l0_error_to_string(result));
			continue;
		}
		DBG("Restored standby mode for domain {} from never to {}.\n", i, promoModeName(savedModes[i]));
	}
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