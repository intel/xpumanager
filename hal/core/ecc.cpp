/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "ecc.h"

/**
 * @brief Prints the ECC (Error Correcting Code) state in human-readable format
 *
 * This utility function decodes and displays the ECC state for debugging
 * and informational purposes, showing whether ECC is enabled, disabled,
 * or unavailable on the device.
 *
 * @param state The ECC state enumeration value to decode and print
 */
std::string ecc::printEccState(const zes_device_ecc_state_t state)
{
	std::string eccStateString;

	switch (state) {
	case ZES_DEVICE_ECC_STATE_UNAVAILABLE:
		eccStateString = "Unavailable";
		break;
	case ZES_DEVICE_ECC_STATE_ENABLED:
		eccStateString = "Enabled";
		break;
	case ZES_DEVICE_ECC_STATE_DISABLED:
		eccStateString = "Disabled";
		break;
	default:
		eccStateString = "Other";
		break;
	}
	DBG("{}\n", eccStateString.c_str());
	return eccStateString;
}

/**
 * @brief Prints the ECC pending action in human-readable format
 *
 * This utility function decodes and displays pending actions required
 * for ECC configuration changes, such as system restart requirements.
 *
 * @param action The pending action enumeration value to decode and print
 */
std::string ecc::printEccPendingAction(const zes_device_action_t action)
{
	std::string eccPendingActionString;
	switch (action) {
	case ZES_DEVICE_ACTION_NONE:
		eccPendingActionString = "None";
		break;
	case ZES_DEVICE_ACTION_WARM_CARD_RESET:
		eccPendingActionString = "Warm reset of the card";
		break;
	case ZES_DEVICE_ACTION_COLD_CARD_RESET:
		eccPendingActionString = "Cold reset of the card";
		break;
	case ZES_DEVICE_ACTION_COLD_SYSTEM_REBOOT:
		eccPendingActionString = "Cold reboot of the system";
		break;
	default:
		eccPendingActionString = "Other";
		break;
	}
	DBG("{}\n", eccPendingActionString.c_str());
	return eccPendingActionString;
}

/**
 * @brief Checks if ECC functionality is available on the device
 *
 * This function determines whether Error Correcting Code (ECC) memory
 * protection is available and supported on the specified device.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return bool True if ECC is available, false otherwise
 */
bool ecc::available(zes_device_handle_t device)
{
	ze_result_t result;
	ze_bool_t eccAvailable;
	result = zesDeviceEccAvailable(device, &eccAvailable);
	if (result != ZE_RESULT_SUCCESS) {
		if (result == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
			// Device simply does not implement ECC; this is benign and expected.
			DBG("ECC availability query unsupported on this device: 0x{:X}\n", result);
		} else {
			ERR("Failed to check ECC availability: 0x{:X} ({})\n", result, l0_error_to_string(result));
		}
		return false;
	}
	return (bool)eccAvailable;
}

/**
 * @brief Checks if ECC configuration can be modified on the device
 *
 * This function determines whether ECC settings can be changed on the
 * specified device, indicating if the device supports dynamic ECC configuration.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return bool True if ECC is configurable, false otherwise
 */
bool ecc::configurable(zes_device_handle_t device)
{
	ze_result_t result;
	ze_bool_t eccConfigurable;
	result = zesDeviceEccConfigurable(device, &eccConfigurable);
	if (result != ZE_RESULT_SUCCESS) {
		if (result == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
			// Device simply does not implement ECC; this is benign and expected.
			DBG("ECC configurability query unsupported on this device: 0x{:X}\n", result);
		} else {
			ERR("Failed to check ECC configurability: 0x{:X} ({})\n", result, l0_error_to_string(result));
		}
		return false;
	}
	return (bool)eccConfigurable;
}

/**
 * @brief Gets the current ECC state and configuration for a device
 *
 * This function retrieves the current ECC state information including
 * current state, pending state changes, and any required actions for
 * ECC configuration modifications.
 *
 * @param device Handle to the Level Zero Sysman device
 * @param eccState[out] Pointer to structure to store ECC state information
 * @return ze_result_t ZE_RESULT_SUCCESS on successful state retrieval, error code otherwise
 */
ze_result_t ecc::getState(zes_device_handle_t device, zes_device_ecc_properties_t *eccState)
{
	TRACING();
	if (!available(device)) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t result = zesDeviceGetEccState(device, eccState);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get ECC state: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	} else {
		DBG("  - ECC State:\n");
		DBG("    - Current State:\n");
		printEccState(eccState->currentState);
		DBG("    - Pending State:\n");
		printEccState(eccState->pendingState);
		DBG("     - Pending Action:\n");
		printEccPendingAction(eccState->pendingAction);
	}
	return result;
}

/**
 * @brief Sets the ECC state for a device
 *
 * This function configures the ECC (Error Correcting Code) state on the
 * specified device, enabling or disabling memory error correction. The
 * change may require a system restart to take effect.
 *
 * @param device Handle to the Level Zero Sysman device
 * @param enable Boolean flag to enable (true) or disable (false) ECC
 * @return ze_result_t ZE_RESULT_SUCCESS if ECC state set successfully, error code otherwise
 */
ze_result_t ecc::setState(zes_device_handle_t device, bool enable, ecc_state_t *state)
{
	TRACING();
	bool eccAvailable = available(device);
	bool eccConfigurable = configurable(device);
	if (!eccAvailable) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	if (!eccConfigurable) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	zes_device_ecc_desc_t newState = {};
	zes_device_ecc_properties_t pState = {};
	newState.state = enable ? ZES_DEVICE_ECC_STATE_ENABLED : ZES_DEVICE_ECC_STATE_DISABLED;
	ze_result_t result = zesDeviceSetEccState(device, &newState, &pState);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to set ECC state: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	if (state != nullptr) {
		state->configurable = eccConfigurable;
		state->available = eccAvailable;
		state->currentState = pState.currentState;
		state->pendingState = pState.pendingState;
		state->pendingAction = pState.pendingAction;
	}

	DBG("  - ECC State:\n");
	DBG("    - Current State:\n");
	printEccState(pState.currentState);
	DBG("    - Pending State:\n");
	printEccState(pState.pendingState);
	DBG("     - Pending Action:\n");
	printEccPendingAction(pState.pendingAction);
	return result;
}

/**
 * @brief Executes ECC monitoring and validation operations
 *
 * This function performs comprehensive ECC assessment by checking availability,
 * configurability, and retrieving current state information. It validates that
 * ECC functionality is supported before attempting to access ECC state.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS if ECC operations completed successfully,
 *                    ZE_RESULT_ERROR_UNSUPPORTED_FEATURE if ECC not available/configurable,
 *                    error code otherwise
 */
ze_result_t ecc::zesRun(zes_device_handle_t device)
{
	TRACING();
	zes_device_ecc_properties_t eccState = {};

	if (!available(device)) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	if (!configurable(device)) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	return getState(device, &eccState);
}
