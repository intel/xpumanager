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

/**
 * @brief Destructor for the power class
 *
 * This destructor performs cleanup operations for the power management
 * object, releasing allocated memory for power domain handles and ensuring
 * proper resource deallocation when the power object is destroyed.
 */
power::~power()
{
	if (powerHandles) {
		delete[] powerHandles;
		powerHandles = nullptr;
	}
}

/**
 * @brief Enumerates available power domains for a device
 *
 * This function discovers and catalogs all power domains available on the
 * specified device. Power domains represent different power management
 * zones that can be monitored and controlled independently.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
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

/**
 * @brief Gets properties for a specific power domain
 *
 * This function retrieves detailed properties and capabilities for a
 * specific power domain, including supported power limits and thresholds.
 *
 * @param powerHandle Handle to the specific power domain
 * @param properties Pointer to structure to store power domain properties
 * @param extProps Pointer to structure to store extended power domain properties
 * @return ze_result_t ZE_RESULT_SUCCESS on successful property retrieval, error code otherwise
 */
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

/**
 * @brief Gets the energy threshold for a power domain
 *
 * This function retrieves the energy threshold settings for the specified
 * power domain, which determines when energy-related events are triggered.
 *
 * @param powerHandle Handle to the specific power domain
 * @return ze_result_t ZE_RESULT_SUCCESS on successful threshold retrieval, error code otherwise
 */
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

/**
 * @brief Gets the current energy counter for a power domain
 *
 * This function retrieves the current energy consumption counter value
 * for the specified power domain, providing accumulated energy usage data.
 *
 * @param powerHandle Handle to the specific power domain
 * @param energyCounter Pointer to structure to store energy counter data
 * @return ze_result_t ZE_RESULT_SUCCESS on successful counter retrieval, error code otherwise
 */
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

/**
 * @brief Gets the current power limits for a power domain
 *
 * This function retrieves the current power limit configuration for the
 * specified power domain, including both sustained and burst power limits.
 *
 * @param powerHandle Handle to the specific power domain
 * @return ze_result_t ZE_RESULT_SUCCESS on successful limit retrieval, error code otherwise
 */
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

/**
 * @brief Sets the power limit for the device
 *
 * This function applies a new power limit setting to the device's primary
 * power domain, controlling the maximum sustained power consumption.
 *
 * @param powerLimit The new power limit value in watts
 * @return ze_result_t ZE_RESULT_SUCCESS if power limit set successfully, error code otherwise
 */
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

/**
 * @brief Gets energy consumption data for specific power domains
 *
 * This function retrieves energy consumption information from either GPU or
 * card-level power domains, providing accumulated energy usage and timestamp
 * data for power analysis and monitoring purposes.
 *
 * @param pwr Pointer to store the energy consumption value (in joules)
 * @param timeStamp Pointer to store the timestamp of the energy reading
 * @param forGPU Flag indicating whether to get GPU domain (true) or card domain (false) energy
 * @return ze_result_t ZE_RESULT_SUCCESS on successful energy retrieval, error code otherwise
 */
ze_result_t power::getEnergy(uint64_t *pwr, uint64_t *timeStamp, bool forGPU)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_power_energy_counter_t energyCounter = {};
	zes_power_properties_t properties;
	zes_power_ext_properties_t extProps;
	zes_power_domain_t domain = forGPU ? ZES_POWER_DOMAIN_GPU : ZES_POWER_DOMAIN_CARD;

	if (pwr == nullptr || timeStamp == nullptr) {
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

		*pwr = energyCounter.energy;
		*timeStamp = energyCounter.timestamp;
		break;
	}

	return result;
}

/**
 * @brief Initializes the power management module for a device
 *
 * This function performs initial setup of power monitoring capabilities by
 * enumerating all available power domains and retrieving their power limits
 * for subsequent power management and monitoring operations.
 *
 * @param device Handle to the device for power initialization
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
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

/**
 * @brief Performs comprehensive power monitoring runtime operations
 *
 * This function executes a complete power monitoring cycle for all power domains,
 * including property retrieval, energy counter reading, threshold checking, and
 * power limit analysis for comprehensive power management assessment.
 *
 * @param device Handle to the device (unused in current implementation)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful execution, error code otherwise
 */
ze_result_t power::zesRun(UNUSED zes_device_handle_t device)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_power_energy_counter_t energyCounter;
	zes_power_properties_t properties;
	zes_power_ext_properties_t extProps;

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