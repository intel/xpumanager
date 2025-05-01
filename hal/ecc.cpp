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
#include "ecc.h"

void ecc::print_ecc_state(const zes_device_ecc_state_t state)
{
	switch (state)
	{
	case ZES_DEVICE_ECC_STATE_UNAVAILABLE:
		DBG("Unavailable\n");
		break;
	case ZES_DEVICE_ECC_STATE_ENABLED:
		DBG("Enabled\n");
		break;
	case ZES_DEVICE_ECC_STATE_DISABLED:
		DBG("Disabled\n");
		break;
	default:
		DBG("Other\n");
		break;
	}
}

void ecc::print_ecc_pending_action(const zes_device_action_t action)
{
	switch (action)
	{
	case ZES_DEVICE_ACTION_NONE:
		DBG("None\n");
		break;
	case ZES_DEVICE_ACTION_WARM_CARD_RESET:
		DBG("Warm reset of the card\n");
		break;
	case ZES_DEVICE_ACTION_COLD_CARD_RESET:
		DBG("Cold reset of the card\n");
		break;
	case ZES_DEVICE_ACTION_COLD_SYSTEM_REBOOT:
		DBG("Cold reboot of the system\n");
		break;
	default:
		DBG("Other\n");
		break;
	}
}

bool ecc::available(zes_device_handle_t device)
{
	ze_result_t result;
	ze_bool_t eccAvailable;
	result = zesDeviceEccAvailable(device, &eccAvailable);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to check ECC availability: 0x%X (%s)\n", result, l0_error_to_string(result));
		return false;
	}
	return (bool)eccAvailable;
}

bool ecc::configurable(zes_device_handle_t device)
{
	ze_result_t result;
	ze_bool_t eccConfigurable;
	result = zesDeviceEccConfigurable(device, &eccConfigurable);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to check ECC configurability: 0x%X (%s)\n", result, l0_error_to_string(result));
		return false;
	}
	return (bool)eccConfigurable;
}

ze_result_t ecc::getState(zes_device_handle_t device)
{
	zes_device_ecc_properties_t eccState = {};
	ze_result_t result = zesDeviceGetEccState(device, &eccState);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get ECC state: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	else
	{
		DBG("  - ECC State:\n");
		DBG("    - Current State:\n");
		print_ecc_state(eccState.currentState);
		DBG("    - Pending State:\n");
		print_ecc_state(eccState.pendingState);
		DBG("     - Pending Action:\n");
		print_ecc_pending_action(eccState.pendingAction);
	}
	return result;
}

ze_result_t ecc::zesRun(zes_device_handle_t device)
{
	if (!available(device))
	{
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	if (!configurable(device))
	{
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	return getState(device);
}