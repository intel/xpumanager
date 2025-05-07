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
#include "powerlimits.h"

powerlimits::~powerlimits()
{
	if (powerHandles)
	{
		delete[] powerHandles;
		powerHandles = nullptr;
	}
}

ze_result_t powerlimits::enumPowerDomains(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumPowerDomains(device, &powerCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || powerCount == 0)
	{
		ERR("Failed to enumerate power domains. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	powerHandles = new zes_pwr_handle_t[powerCount];
	result = zesDeviceEnumPowerDomains(device, &powerCount, powerHandles);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get power domains. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Found %d power domains.\n", powerCount);
	return result;
}

ze_result_t powerlimits::getProperties(zes_pwr_handle_t powerHandle)
{
	zes_power_properties_t properties = {};
	ze_result_t result = zesPowerGetProperties(powerHandle, &properties);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get properties for power domain 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Power Properties:\n");
	DBG("  Default Limit: %d W\n", properties.defaultLimit);
	DBG("  Max Limit: %d W\n", properties.maxLimit);
	DBG("  Min Limit: %d W\n", properties.minLimit);
	DBG("  Is Energy threshold supported: %d\n", properties.isEnergyThresholdSupported);
	DBG("  Can control: %d\n", properties.canControl);
	DBG("  onSubdevice: %d\n", properties.onSubdevice);
	DBG("  subdeviceId: %d\n", properties.subdeviceId);

	return result;
}

ze_result_t powerlimits::getEnergyCounter(zes_pwr_handle_t powerHandle)
{
	zes_power_energy_counter_t energyCounter;
	ze_result_t result = zesPowerGetEnergyCounter(powerHandle, &energyCounter);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get energy counter. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Energy Counter:\n");
	DBG("  Energy: %" PRIu64 "J\n", energyCounter.energy);
	DBG("  Timestamp: %" PRIu64 "us\n", energyCounter.timestamp);

	return result;
}

ze_result_t powerlimits::getEnergyThreshold(zes_pwr_handle_t powerHandle)
{
	zes_energy_threshold_t energyThreshold;
	ze_result_t result = zesPowerGetEnergyThreshold(powerHandle, &energyThreshold);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get energy threshold. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Energy Threshold:\n");
	DBG("  Enabled: %d\n", energyThreshold.enable);
	DBG("  Threshold: %f J\n", energyThreshold.threshold);

	return result;
}

ze_result_t powerlimits::zesRun(zes_device_handle_t device)
{
	ze_result_t result = enumPowerDomains(device);
	if (result != ZE_RESULT_SUCCESS)
	{
		return result;
	}

	for (uint32_t i = 0; i < powerCount; ++i)
	{
		result = getProperties(powerHandles[i]);
		if (result != ZE_RESULT_SUCCESS)
		{
			return result;
		}

		result = getEnergyCounter(powerHandles[i]);
		if (result != ZE_RESULT_SUCCESS)
		{
			return result;
		}

		result = getEnergyThreshold(powerHandles[i]);
		if (result != ZE_RESULT_SUCCESS)
		{
			return result;
		}
	}

	return result;
}