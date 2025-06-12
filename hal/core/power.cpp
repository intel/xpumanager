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
#include "power.h"

power::~power()
{
	if (powerHandles) {
		delete[] powerHandles;
		powerHandles = nullptr;
	}
}

ze_result_t power::enumPowerDomains(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumPowerDomains(device, &powerCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || powerCount == 0) {
		ERR("Failed to enumerate power domains. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	powerHandles = new zes_pwr_handle_t[powerCount];
	result = zesDeviceEnumPowerDomains(device, &powerCount, powerHandles);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get power domains. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Found %d power domains.\n", powerCount);
	return result;
}

ze_result_t power::getProperties(zes_pwr_handle_t powerHandle, zes_power_properties_t *properties,
								 zes_power_ext_properties_t *extProps)
{
	memset(properties, 0, sizeof(zes_power_properties_t));
	memset(extProps, 0, sizeof(zes_power_ext_properties_t));
	properties->pNext = extProps;
	properties->stype = ZES_STRUCTURE_TYPE_POWER_PROPERTIES;
	extProps->stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;

	ze_result_t result = zesPowerGetProperties(powerHandle, properties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get properties for power domain 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Power Properties:\n");
	DBG("  Default Limit: %d W\n", properties->defaultLimit);
	DBG("  Max Limit: %d W\n", properties->maxLimit);
	DBG("  Min Limit: %d W\n", properties->minLimit);
	DBG("  Is Energy threshold supported: %d\n", properties->isEnergyThresholdSupported);
	DBG("  Can control: %d\n", properties->canControl);
	DBG("  onSubdevice: %d\n", properties->onSubdevice);
	DBG("  subdeviceId: %d\n", properties->subdeviceId);
	DBG("  extProperties:\n");
	DBG("    Domain: %d\n", extProps->domain);
	switch (extProps->domain) {
	case ZES_POWER_DOMAIN_UNKNOWN:
		DBG("    Domain Type: Unknown\n");
		break;
	case ZES_POWER_DOMAIN_CARD:
		DBG("    Domain Type: Card\n");
		break;
	case ZES_POWER_DOMAIN_PACKAGE:
		DBG("    Domain Type: Package\n");
		break;
	case ZES_POWER_DOMAIN_STACK:
		DBG("    Domain Type: Stack\n");
		break;
	case ZES_POWER_DOMAIN_MEMORY:
		DBG("    Domain Type: Memory\n");
		break;
	case ZES_POWER_DOMAIN_GPU:
		DBG("    Domain Type: GPU\n");
		break;
	default:
		DBG("    Domain Type: Unknown\n");
		break;
	}

	return result;
}

ze_result_t power::getEnergyThreshold(zes_pwr_handle_t powerHandle)
{
	zes_energy_threshold_t energyThreshold;
	ze_result_t result = zesPowerGetEnergyThreshold(powerHandle, &energyThreshold);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get energy threshold. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Energy Threshold:\n");
	DBG("  Enabled: %d\n", energyThreshold.enable);
	DBG("  Threshold: %f J\n", energyThreshold.threshold);
	DBG("  Process ID: %u\n", energyThreshold.processId);

	return result;
}

ze_result_t power::getEnergyCounter(zes_pwr_handle_t powerHandle, zes_power_energy_counter_t *energyCounter)
{
	memset(energyCounter, 0, sizeof(zes_power_energy_counter_t));
	ze_result_t result = zesPowerGetEnergyCounter(powerHandle, energyCounter);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get energy counter. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Energy Counter:\n");
	DBG("  Energy: %" PRIu64 " J\n", energyCounter->energy);
	DBG("  Timestamp: %" PRIu64 " us\n", energyCounter->timestamp);

	return result;
}

ze_result_t power::getPowerLimits(zes_pwr_handle_t powerHandle)
{
	uint32_t powerLimitsCount = 0;
	ze_result_t result = zesPowerGetLimitsExt(powerHandle, &powerLimitsCount, NULL);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get extended power limits. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	zes_power_limit_ext_desc_t *powerLimits = new zes_power_limit_ext_desc_t[powerLimitsCount];
	result = zesPowerGetLimitsExt(powerHandle, &powerLimitsCount, powerLimits);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get power limits. 0x%X (%s)\n", result, l0_error_to_string(result));
		delete[] powerLimits;
		return result;
	}

	DBG("Power Limits Count: %d\n", powerLimitsCount);
	DBG("Power Limits:\n");
	for (uint32_t i = 0; i < powerLimitsCount; ++i) {
		DBG("  Limit %d:\n", i);
		DBG("    Source: %d\n", powerLimits[i].source);
		DBG("    Limit: %d mW\n", powerLimits[i].limit);
		DBG("    Interval: %d ms\n", powerLimits[i].interval);
	}

	delete[] powerLimits;

	return result;
}

ze_result_t power::setPowerLimit(double powerLimit)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	uint32_t powerLimitsCount;

	for (uint32_t i = 0; i < powerCount; ++i) {
		result = zesPowerGetLimitsExt(powerHandles[i], &powerLimitsCount, NULL);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get extended power limits. 0x%X (%s)\n", result, l0_error_to_string(result));
			return result;
		}

		zes_power_limit_ext_desc_t *powerLimits = new zes_power_limit_ext_desc_t[powerLimitsCount];
		result = zesPowerGetLimitsExt(powerHandles[i], &powerLimitsCount, powerLimits);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get power limits. 0x%X (%s)\n", result, l0_error_to_string(result));
			delete[] powerLimits;
			return result;
		}

		for (uint32_t j = 0; j < powerLimitsCount; ++j) {
			powerLimits[j].limit = static_cast<uint32_t>(powerLimit * 1000); // Convert to mW
		}

		result = zesPowerSetLimitsExt(powerHandles[i], &powerLimitsCount, powerLimits);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to set power limits. 0x%X (%s)\n", result, l0_error_to_string(result));
			delete[] powerLimits;
			return result;
		}
		DBG("Successfully set power limits:\n");
		delete[] powerLimits;
	}
	return result;
}

ze_result_t power::getPower(uint64_t *power, uint64_t *timeStamp, bool forGPU)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_power_energy_counter_t energyCounter = {};
	zes_power_properties_t properties;
	zes_power_ext_properties_t extProps;
	zes_power_domain_t domain = forGPU ? ZES_POWER_DOMAIN_GPU : ZES_POWER_DOMAIN_CARD;

	if (power == nullptr || timeStamp == nullptr) {
		ERR("Power or timestamp pointer is null.\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}

	for (uint32_t i = 0; i < powerCount; ++i) {
		// First we are supposed to get the properties of the power domain. This is so that we can check if the domain
		// matches the one we are looking for (GPU or CARD).
		result = getProperties(powerHandles[i], &properties, &extProps);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		// Skip if not the desired domain or if powerCount is only 1 (as is the case in some GPUs), then look for
		// package domain
		if (extProps.domain != domain && (powerCount != 1 || extProps.domain != ZES_POWER_DOMAIN_PACKAGE)) {
			continue;
		}

		// Once we found the right domain, we can get the energy counter
		result = getEnergyCounter(powerHandles[i], &energyCounter);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get energy counter for power domain %d. 0x%X (%s)\n", i, result, l0_error_to_string(result));
			return result;
		}

		*power = energyCounter.energy;
		*timeStamp = energyCounter.timestamp;
		break;
	}

	return result;
}

ze_result_t power::init(zes_device_handle_t device)
{
	ze_result_t result = enumPowerDomains(device);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	for (uint32_t i = 0; i < powerCount; ++i) {
		result = getPowerLimits(powerHandles[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
	}
	return result;
}

ze_result_t power::zesRun(zes_device_handle_t device)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_power_energy_counter_t energyCounter;
	zes_power_properties_t properties;
	zes_power_ext_properties_t extProps;
	UNUSED(device);

	for (uint32_t i = 0; i < powerCount; ++i) {
		result = getProperties(powerHandles[i], &properties, &extProps);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
		result = getEnergyCounter(powerHandles[i], &energyCounter);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		getEnergyThreshold(powerHandles[i]);

		result = getPowerLimits(powerHandles[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
	}
	return result;
}