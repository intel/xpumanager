/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "power.h"
#include "device.h"
#include <map>
#include <memory>

/**
 * @brief Helper function to parse device ID from hexadecimal string
 *
 * @param hexKey Hexadecimal string representation of device ID
 * @return uint32_t Parsed device ID
 */
uint32_t power::parseDeviceId(const std::string &hexKey) { return (uint32_t)std::stoul(hexKey, nullptr, 16); }

/**
 * @brief Helper function to load threshold data from JSON into a map
 *
 * This function checks if the JSON object contains the specified key,
 * and if so, iterates through its items to populate the target map
 * with device ID to threshold value mappings.
 *
 * @param thresholdsJson JSON object containing threshold data
 * @param key Key to look for in the JSON object
 * @param setter Function to call for each device threshold pair
 */
void power::loadThresholdSection(const nlohmann::json &thresholdsJson, const std::string &key,
								 std::function<void(uint32_t, uint64_t)> setter)
{
	if (thresholdsJson.contains(key)) {
		for (const auto &[deviceKey, value] : thresholdsJson[key].items()) {
			uint32_t deviceId = parseDeviceId(deviceKey);
			setter(deviceId, value.get<uint64_t>());
		}
	}
}

/**
 * @brief Constructor for the power class
 *
 * Initializes the power management object with default values and
 * loads power thresholds from configuration.
 */
power::power()
	: powerCount(0), powerHandles(nullptr), zeDeviceHandle(nullptr), deviceHandle(nullptr),
	  thresholds(new PowerThresholds()), defaultThrottlePower(DEFAULT_THROTTLE_POWER)
{
	loadPowerThresholds();
}

/**
 * @brief Loads power thresholds from configuration file
 *
 * This function reads power thresholds from the configuration file
 * device_thresholds.json and populates the device-specific TDP maps.
 * If the file cannot be read or parsed, default values are used.
 */
void power::loadPowerThresholds()
{
	try {
		std::ifstream configFile(std::string(XPUM_CONFIG_DIR) + std::string("device_thresholds.json"));

		if (configFile.is_open()) {
			nlohmann::json configJson;
			configFile >> configJson;
			configFile.close();

			if (configJson.contains("default_thresholds") && configJson["default_thresholds"].contains("power_tdp")) {
				defaultThrottlePower = configJson["default_thresholds"]["power_tdp"].get<uint64_t>();
			}

			if (configJson.contains("power_thresholds")) {
				auto powerThresholdsJson = configJson["power_thresholds"];
				loadThresholdSection(powerThresholdsJson, "tdps", [this](uint32_t deviceId, uint64_t value) {
					thresholds->setThrottlePower(deviceId, value);
				});
			} else {
				DBG("Power thresholds config file section not found, using defaults\n");
			}
		}
	} catch (const std::exception &e) {
		ERR("Error loading power thresholds config: {}\n", e.what());
		DBG("Using default power thresholds\n");
	}
}

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
	delete thresholds;
	thresholds = nullptr;
}

/**
 * @brief PowerThresholds getter method
 */
uint64_t PowerThresholds::getThrottlePower(uint32_t deviceId, uint64_t defaultValue) const
{
	auto it = healthDeviceToTdps.find(deviceId);
	return (it != healthDeviceToTdps.end()) ? it->second : defaultValue;
}

/**
 * @brief PowerThresholds setter method
 */
void PowerThresholds::setThrottlePower(uint32_t deviceId, uint64_t value) { healthDeviceToTdps[deviceId] = value; }

/**
 * @brief Gets the throttle power limit for a specific device
 *
 * This function retrieves the device-specific power throttle threshold that
 * determines when power-based throttling should begin for thermal and power
 * management. Returns a device-specific value if configured, otherwise returns
 * the default throttle power of 300 watts.
 *
 * @param pciDeviceId PCI device ID to lookup device-specific threshold
 * @return uint64_t Throttle power limit in watts
 */
uint64_t power::getThrottlePower(uint32_t pciDeviceId)
{
	return thresholds->getThrottlePower(pciDeviceId, defaultThrottlePower);
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
		ERR("Failed to enumerate power domains. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	powerHandles = new zes_pwr_handle_t[powerCount];
	result = zesDeviceEnumPowerDomains(device, &powerCount, powerHandles);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get power domains. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Found {} power domains.\n", powerCount);
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
		ERR("Failed to get properties for power domain 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Power Properties:\n");
	DBG("  Default Limit: {} W\n", properties->defaultLimit);
	DBG("  Max Limit: {} W\n", properties->maxLimit);
	DBG("  Min Limit: {} W\n", properties->minLimit);
	DBG("  Is Energy threshold supported: {}\n", properties->isEnergyThresholdSupported);
	DBG("  Can control: {}\n", properties->canControl);
	DBG("  onSubdevice: {}\n", properties->onSubdevice);
	DBG("  subdeviceId: {}\n", properties->subdeviceId);
	DBG("  extProperties:\n");
	DBG("    Domain: {}\n", extProps->domain);
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
		ERR("Failed to get energy threshold. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Energy Threshold:\n");
	DBG("  Enabled: {}\n", energyThreshold.enable);
	DBG("  Threshold: {:f} J\n", energyThreshold.threshold);
	DBG("  Process ID: {}\n", energyThreshold.processId);

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
		DBG("Failed to get energy counter. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Energy Counter:\n");
	DBG("  Energy: {} J\n", energyCounter->energy);
	DBG("  Timestamp: {} us\n", energyCounter->timestamp);

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
		ERR("Failed to get extended power limits. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	std::vector<zes_power_limit_ext_desc_t> powerLimits(powerLimitsCount);
	result = zesPowerGetLimitsExt(powerHandle, &powerLimitsCount, powerLimits.data());
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get power limits. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Power Limits Count: {}\n", powerLimitsCount);
	DBG("Power Limits:\n");
	for (uint32_t i = 0; i < powerLimitsCount; ++i) {
		DBG("  Limit {}:\n", i);
		DBG("    Source: {}\n", powerLimits[i].source);
		DBG("    Limit: {} mW\n", powerLimits[i].limit);
		DBG("    Interval: {} ms\n", powerLimits[i].interval);
	}

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
			ERR("Failed to get extended power limits. 0x{:X} ({})\n", result, l0_error_to_string(result));
			return result;
		}

		std::vector<zes_power_limit_ext_desc_t> powerLimits(powerLimitsCount);
		result = zesPowerGetLimitsExt(powerHandles[i], &powerLimitsCount, powerLimits.data());
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get power limits. 0x{:X} ({})\n", result, l0_error_to_string(result));
			return result;
		}

		for (uint32_t j = 0; j < powerLimitsCount; ++j) {
			powerLimits[j].limit = static_cast<uint32_t>(powerLimit * 1000); // Convert to mW
		}

		result = zesPowerSetLimitsExt(powerHandles[i], &powerLimitsCount, powerLimits.data());
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to set power limits. 0x{:X} ({})\n", result, l0_error_to_string(result));
			return result;
		}
		DBG("Successfully set power limits:\n");
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
			DBG("Failed to get energy counter for power domain {}. 0x{:X} ({})\n", i, result,
				l0_error_to_string(result));
			return result;
		}

		*pwr = energyCounter.energy;
		*timeStamp = energyCounter.timestamp;
		break;
	}

	return result;
}

/**
 * @brief Gets energy counters for each tile/subdevice
 *
 * This function enumerates all power domains and returns energy counter data
 * for each subdevice/tile. It maps subdevice IDs to their corresponding
 * energy and timestamp values. Used for per-tile power monitoring.
 *
 * For multi-tile GPUs with subdevice power domains, returns per-tile data.
 * For single-tile GPUs or GPUs that only expose CARD-level power, returns
 * the card-level data as tile 0.
 *
 * @param [out] tileEnergy Map of tile_id -> (energy_microjoules, timestamp_microseconds)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful energy retrieval, error code otherwise
 */
ze_result_t power::getEnergyPerTile(std::map<uint32_t, std::pair<uint64_t, uint64_t>> &tileEnergy)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	tileEnergy.clear();

	// First pass: look for subdevice-level power domains (multi-tile GPUs)
	bool foundSubdevicePower = false;
	for (uint32_t i = 0; i < powerCount; ++i) {
		zes_power_properties_t properties = {};
		zes_power_ext_properties_t extProps = {};
		result = getProperties(powerHandles[i], &properties, &extProps);
		if (result != ZE_RESULT_SUCCESS) {
			DBG("Failed to get properties for power domain {}\n", i);
			continue;
		}

		if (properties.onSubdevice) {
			zes_power_energy_counter_t energyCounter = {};
			result = getEnergyCounter(powerHandles[i], &energyCounter);
			if (result != ZE_RESULT_SUCCESS) {
				DBG("Failed to get energy counter for subdevice power domain {}: 0x{:X} ({})\n", i, result,
					l0_error_to_string(result));
				continue;
			}

			uint32_t tileId = properties.subdeviceId;
			tileEnergy[tileId] = std::make_pair(energyCounter.energy, energyCounter.timestamp);
			DBG("Tile {} (subdevice) energy: {} µJ, timestamp: {} µs\n", tileId, energyCounter.energy,
				energyCounter.timestamp);
			foundSubdevicePower = true;
		}
	}

	if (foundSubdevicePower) {
		return ZE_RESULT_SUCCESS;
	}

	// Second pass: look for CARD or PACKAGE level power
	for (uint32_t i = 0; i < powerCount; ++i) {
		zes_power_properties_t properties = {};
		zes_power_ext_properties_t extProps = {};
		result = getProperties(powerHandles[i], &properties, &extProps);
		if (result != ZE_RESULT_SUCCESS) {
			continue;
		}

		if (extProps.domain == ZES_POWER_DOMAIN_CARD || extProps.domain == ZES_POWER_DOMAIN_PACKAGE ||
			extProps.domain == ZES_POWER_DOMAIN_GPU) {
			zes_power_energy_counter_t energyCounter = {};
			result = getEnergyCounter(powerHandles[i], &energyCounter);
			if (result != ZE_RESULT_SUCCESS) {
				DBG("Failed to get energy counter for power domain {}: 0x{:X} ({})\n", i, result,
					l0_error_to_string(result));
				continue;
			}

			tileEnergy[0] = std::make_pair(energyCounter.energy, energyCounter.timestamp);
			DBG("Tile 0 (device-level, domain={}) energy: {} µJ, timestamp: {} µs\n", extProps.domain,
				energyCounter.energy, energyCounter.timestamp);
			break; // Only need one device-level reading
		}
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Sets the sustained power limit for a device or specific tile
 *
 * This function configures the sustained power limit, which controls the
 * long-term average power consumption. The limit can be applied to either
 * the entire device or a specific tile/subdevice.
 *
 * @param limitMw The sustained power limit in milliwatts
 * @param tileId Tile identifier (-1 for device level, >= 0 for specific tile)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful limit configuration, error code otherwise
 */
ze_result_t power::setSustainedLimit(uint32_t limitMw, int32_t tileId)
{
	// Get subdevice mapping if tileId is specified
	uint32_t targetSubdeviceId = 0;
	bool isSubdeviceTarget = false;

	if (tileId != -1) {
		if (deviceHandle == nullptr) {
			ERR("Parent device not set. Cannot map tile to subdevice.\n");
			return ZE_RESULT_ERROR_UNINITIALIZED;
		}

		zes_subdevice_exp_properties_t subdeviceProps = {};
		ze_result_t res = deviceHandle->getSubdeviceProperties(static_cast<uint32_t>(tileId), subdeviceProps);
		if (res != ZE_RESULT_SUCCESS) {
			return res;
		}
		targetSubdeviceId = subdeviceProps.subdeviceId;
		isSubdeviceTarget = true;
	}

	for (uint32_t i = 0; i < powerCount; ++i) {
		zes_power_properties_t props = {};
		props.stype = ZES_STRUCTURE_TYPE_POWER_PROPERTIES;
		props.pNext = nullptr;
		ze_result_t res = zesPowerGetProperties(powerHandles[i], &props);
		if (res != ZE_RESULT_SUCCESS) {
			ERR("Failed to get power properties. 0x{:X} ({})\n", res, l0_error_to_string(res));
			continue;
		}

		// Match based on tileId mapping or device level
		bool isMatch = false;
		if (isSubdeviceTarget) {
			// Match specific subdevice
			isMatch = (props.onSubdevice && props.subdeviceId == targetSubdeviceId);
		} else {
			// Match device-level (root device)
			isMatch = !props.onSubdevice;
		}

		if (isMatch) {
			zes_power_sustained_limit_t sustained = {};
			sustained.enabled = true;
			sustained.power = limitMw;
			sustained.interval = 0; // Use default

			res = zesPowerSetLimits(powerHandles[i], &sustained, nullptr, nullptr);
			if (res == ZE_RESULT_SUCCESS) {
				return res;
			}
			ERR("Failed to set sustained limit. 0x{:X} ({})\n", res, l0_error_to_string(res));
		}
	}

	ERR("No matching power domain found for tileId {}.\n", tileId);
	return ZE_RESULT_ERROR_UNKNOWN;
}

/**
 * @brief Sets the burst power limit for a device
 *
 * This function configures the burst power limit, which controls the
 * short-term peak power consumption that can be sustained during
 * brief periods of high activity.
 *
 * @param limitMw The burst power limit in milliwatts
 * @return ze_result_t ZE_RESULT_SUCCESS on successful limit configuration, error code otherwise
 */
ze_result_t power::setBurstLimit(uint32_t limitMw)
{
	ze_result_t firstError = ZE_RESULT_ERROR_UNKNOWN;
	bool foundRootDomain = false;

	for (uint32_t i = 0; i < powerCount; ++i) {
		zes_power_properties_t props = {};
		props.stype = ZES_STRUCTURE_TYPE_POWER_PROPERTIES;
		props.pNext = nullptr;
		ze_result_t res = zesPowerGetProperties(powerHandles[i], &props);
		if (res != ZE_RESULT_SUCCESS) {
			if (firstError == ZE_RESULT_ERROR_UNKNOWN) {
				firstError = res;
			}
			continue;
		}

		// Only apply to device-level power domains
		if (!props.onSubdevice) {
			foundRootDomain = true;
			zes_power_burst_limit_t burst = {};
			burst.enabled = true;
			burst.power = limitMw;

			res = zesPowerSetLimits(powerHandles[i], nullptr, &burst, nullptr);
			if (res == ZE_RESULT_SUCCESS) {
				return res;
			}
			if (firstError == ZE_RESULT_ERROR_UNKNOWN) {
				firstError = res;
			}
		}
	}

	if (!foundRootDomain) {
		ERR("No matching device-level power domain found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	if (firstError != ZE_RESULT_ERROR_UNKNOWN) {
		ERR("Failed to set burst limit. 0x{:X} ({})\n", firstError, l0_error_to_string(firstError));
		return firstError;
	}

	return ZE_RESULT_ERROR_UNKNOWN;
}

/**
 * @brief Sets the peak power limits for AC and DC power sources
 *
 * This function configures the peak power limits for both AC and DC
 * power sources, controlling the absolute maximum power consumption
 * from each power source type.
 *
 * @param limitAcMw The AC peak power limit in milliwatts
 * @param limitDcMw The DC peak power limit in milliwatts
 * @return ze_result_t ZE_RESULT_SUCCESS on successful limit configuration, error code otherwise
 */
ze_result_t power::setPeakLimit(uint32_t limitAcMw, uint32_t limitDcMw)
{
	ze_result_t firstError = ZE_RESULT_ERROR_UNKNOWN;
	bool foundRootDomain = false;

	for (uint32_t i = 0; i < powerCount; ++i) {
		zes_power_properties_t props = {};
		props.stype = ZES_STRUCTURE_TYPE_POWER_PROPERTIES;
		props.pNext = nullptr;
		ze_result_t res = zesPowerGetProperties(powerHandles[i], &props);
		if (res != ZE_RESULT_SUCCESS) {
			if (firstError == ZE_RESULT_ERROR_UNKNOWN) {
				firstError = res;
			}
			continue;
		}

		// Only apply to device-level power domains
		if (!props.onSubdevice) {
			foundRootDomain = true;
			zes_power_peak_limit_t peak = {};
			peak.powerAC = limitAcMw;
			peak.powerDC = limitDcMw;

			res = zesPowerSetLimits(powerHandles[i], nullptr, nullptr, &peak);
			if (res == ZE_RESULT_SUCCESS) {
				return res;
			}
			if (firstError == ZE_RESULT_ERROR_UNKNOWN) {
				firstError = res;
			}
		}
	}

	if (!foundRootDomain) {
		ERR("No matching device-level power domain found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	if (firstError != ZE_RESULT_ERROR_UNKNOWN) {
		ERR("Failed to set peak limit. 0x{:X} ({})\n", firstError, l0_error_to_string(firstError));
		return firstError;
	}

	return ZE_RESULT_ERROR_UNKNOWN;
}

/**
 * @brief Gets extended power limit information for all power domains
 *
 * This function retrieves detailed power limit information using the
 * extended power management interface, including power levels, current
 * limits, and lock status for device-level power domains.
 *
 * @param limits Vector to populate with extended power limit information
 * @return ze_result_t ZE_RESULT_SUCCESS on successful retrieval, error code otherwise
 */
ze_result_t power::getLimitsExt(std::vector<PowerLimitExt> &limits)
{
	for (uint32_t i = 0; i < powerCount; ++i) {
		zes_power_properties_t props = {};
		zes_power_ext_properties_t extProps = {};
		zes_power_limit_ext_desc_t defaultLimit = {};

		extProps.defaultLimit = &defaultLimit;
		extProps.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
		props.pNext = &extProps;
		props.stype = ZES_STRUCTURE_TYPE_POWER_PROPERTIES;

		ze_result_t res = zesPowerGetProperties(powerHandles[i], &props);
		if (res != ZE_RESULT_SUCCESS) {
			ERR("Failed to get power properties. 0x{:X} ({})\n", res, l0_error_to_string(res));
			continue;
		}

		if (props.onSubdevice == false) {
			uint32_t limitCount = 0;
			res = zesPowerGetLimitsExt(powerHandles[i], &limitCount, nullptr);
			if (res != ZE_RESULT_SUCCESS) {
				if (res != ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
					ERR("Failed to get extended power limits count. 0x{:X} ({})\n", res, l0_error_to_string(res));
				}
				continue;
			}

			std::vector<zes_power_limit_ext_desc_t> powerExtDescs(limitCount);
			res = zesPowerGetLimitsExt(powerHandles[i], &limitCount, powerExtDescs.data());
			if (res != ZE_RESULT_SUCCESS) {
				ERR("Failed to get extended power limits. 0x{:X} ({})\n", res, l0_error_to_string(res));
				continue;
			}

			for (uint32_t j = 0; j < limitCount; j++) {
				PowerLimitExt limit;
				limit.level = static_cast<zes_power_level_t>(powerExtDescs[j].level);
				limit.limitMw = powerExtDescs[j].limit;
				limit.locked = powerExtDescs[j].limitValueLocked;
				limits.push_back(limit);
			}
		}
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Gets power domain limits for all power domains
 *
 * This function retrieves power limit values organized by power domain and
 * power level, aggregating extended power limit information across all
 * device-level power handles.
 *
 * @param domainLimits Output map of power domain to power level to limit value in milliwatts
 * @return ze_result_t ZE_RESULT_SUCCESS on successful retrieval, error code otherwise
 */
ze_result_t power::getDomainLimits(std::map<zes_power_domain_t, std::map<zes_power_level_t, int32_t>> &domainLimits)
{
	domainLimits.clear();
	for (uint32_t i = 0; i < powerCount; ++i) {
		zes_power_properties_t props = {};
		zes_power_ext_properties_t extProps = {};
		zes_power_limit_ext_desc_t defaultLimit = {};

		extProps.defaultLimit = &defaultLimit;
		extProps.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
		props.pNext = &extProps;
		props.stype = ZES_STRUCTURE_TYPE_POWER_PROPERTIES;

		ze_result_t res = zesPowerGetProperties(powerHandles[i], &props);
		if (res != ZE_RESULT_SUCCESS) {
			DBG("Failed to get power properties for handle {}. 0x{:X} ({})\n", i, res, l0_error_to_string(res));
			continue;
		}
		if (props.onSubdevice) {
			continue;
		}

		uint32_t limitCount = 0;
		res = zesPowerGetLimitsExt(powerHandles[i], &limitCount, nullptr);
		if (res != ZE_RESULT_SUCCESS) {
			DBG("Failed to get power limit count for handle {}. 0x{:X} ({})\n", i, res, l0_error_to_string(res));
			continue;
		}

		std::vector<zes_power_limit_ext_desc_t> powerExtDescs(limitCount);
		res = zesPowerGetLimitsExt(powerHandles[i], &limitCount, powerExtDescs.data());
		if (res != ZE_RESULT_SUCCESS) {
			DBG("Failed to get extended power limits for handle {}. 0x{:X} ({})\n", i, res, l0_error_to_string(res));
			continue;
		}

		for (uint32_t j = 0; j < limitCount; j++) {
			zes_power_level_t level = static_cast<zes_power_level_t>(powerExtDescs[j].level);
			domainLimits[extProps.domain][level] = powerExtDescs[j].limit;
		}
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Gets the maximum power limit as a formatted string range
 *
 * This function queries all power handles to find the maximum power limit
 * and returns it as a formatted string range (e.g., "1 to 300"), preferring
 * the package-level domain value.
 *
 * @param range Output string containing the formatted power limit range in watts
 * @return ze_result_t ZE_RESULT_SUCCESS on successful retrieval, error code otherwise
 */
ze_result_t power::getMaxPowerLimit(std::string &range)
{
	range.clear();
	for (uint32_t i = 0; i < powerCount; ++i) {
		zes_power_properties_t props = {};
		zes_power_ext_properties_t extProps = {};
		props.stype = ZES_STRUCTURE_TYPE_POWER_PROPERTIES;
		props.pNext = &extProps;
		extProps.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
		if (zesPowerGetProperties(powerHandles[i], &props) != ZE_RESULT_SUCCESS) {
			continue;
		}
		if (props.maxLimit > 0) {
			uint32_t maxLimitW = props.maxLimit / 1000;
			range = "1 to " + std::to_string(maxLimitW);
			if (extProps.domain == ZES_POWER_DOMAIN_PACKAGE) {
				break;
			}
		}
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Sets extended power limits for a specific power level
 *
 * This function configures power limits using the extended power management
 * interface, allowing fine-grained control over specific power levels for
 * either the entire device or individual tiles.
 *
 * @param tileId Tile identifier (-1 for device level, >= 0 for specific tile)
 * @param level The power level to configure (e.g., sustained, burst, peak)
 * @param limitMw The power limit in milliwatts
 * @return ze_result_t ZE_RESULT_SUCCESS on successful limit configuration,
 *         ZE_RESULT_ERROR_NOT_AVAILABLE if limit is locked,
 *         ZE_RESULT_ERROR_UNSUPPORTED_FEATURE if level not found,
 *         error code otherwise
 */
ze_result_t power::setLimitsExt(int32_t tileId, zes_power_level_t level, uint32_t limitMw)
{
	// Get subdevice mapping if tileId is specified
	uint32_t targetSubdeviceId = 0;
	bool isSubdeviceTarget = false;

	if (tileId != -1) {
		if (deviceHandle == nullptr) {
			ERR("Parent device not set. Cannot map tile to subdevice.\n");
			return ZE_RESULT_ERROR_UNINITIALIZED;
		}

		zes_subdevice_exp_properties_t subdeviceProps = {};
		ze_result_t res = deviceHandle->getSubdeviceProperties(static_cast<uint32_t>(tileId), subdeviceProps);
		if (res != ZE_RESULT_SUCCESS) {
			return res;
		}
		targetSubdeviceId = subdeviceProps.subdeviceId;
		isSubdeviceTarget = true;
	}

	for (uint32_t i = 0; i < powerCount; ++i) {
		zes_power_properties_t props = {};
		props.stype = ZES_STRUCTURE_TYPE_POWER_PROPERTIES;
		ze_result_t res = zesPowerGetProperties(powerHandles[i], &props);
		if (res != ZE_RESULT_SUCCESS) {
			ERR("Failed to get power properties. 0x{:X} ({})\n", res, l0_error_to_string(res));
			continue;
		}

		// Match based on tileId mapping or device level
		bool isMatch = false;
		if (isSubdeviceTarget) {
			// Match specific subdevice
			isMatch = (props.onSubdevice && props.subdeviceId == targetSubdeviceId);
		} else {
			// Match device-level (root device)
			isMatch = !props.onSubdevice;
		}

		if (isMatch) {
			uint32_t limitCount = 0;
			res = zesPowerGetLimitsExt(powerHandles[i], &limitCount, nullptr);
			if (res != ZE_RESULT_SUCCESS) {
				ERR("Failed to get extended power limits count. 0x{:X} ({})\n", res, l0_error_to_string(res));
				continue;
			}

			std::vector<zes_power_limit_ext_desc_t> powerExtDescs(limitCount);
			res = zesPowerGetLimitsExt(powerHandles[i], &limitCount, powerExtDescs.data());
			if (res != ZE_RESULT_SUCCESS) {
				ERR("Failed to get extended power limits. 0x{:X} ({})\n", res, l0_error_to_string(res));
				continue;
			}

			bool found = false;
			for (uint32_t j = 0; j < limitCount; j++) {
				if (powerExtDescs[j].level == level) {
					if (powerExtDescs[j].limitValueLocked) {
						// Limit is locked, cannot set, return error
						ERR("Power limit for level {} is locked.\n", level);
						return ZE_RESULT_ERROR_NOT_AVAILABLE;
					}
					powerExtDescs[j].limit = limitMw;
					found = true;
					break;
				}
			}

			if (!found) {
				ERR("Power level {} not found.\n", level);
				return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
			}

			res = zesPowerSetLimitsExt(powerHandles[i], &limitCount, powerExtDescs.data());
			if (res != ZE_RESULT_SUCCESS) {
				ERR("Failed to set extended power limits. 0x{:X} ({})\n", res, l0_error_to_string(res));
			}
			return res;
		}
	}

	ERR("No matching power domain found for tileId {}.\n", tileId);
	return ZE_RESULT_ERROR_UNKNOWN;
}

/**
 * @brief Initializes the power management module for a device (legacy version)
 *
 * This function performs initial setup of power monitoring capabilities by
 * enumerating all available power domains and retrieving their power limits.
 * Note: This version does not support tile-specific operations.
 *
 * @param device Handle to the device for power initialization
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t power::init(zes_device_handle_t zeDevice)
{
	zeDeviceHandle = zeDevice;
	deviceHandle = nullptr;

	ze_result_t result = enumPowerDomains(zeDeviceHandle);
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
 * @brief Initializes the power management module for a device with parent device context
 *
 * This function performs initial setup of power monitoring capabilities by
 * enumerating all available power domains and retrieving their power limits.
 * This version supports tile-specific operations through the parent device pointer.
 *
 * @param device Handle to the device for power initialization
 * @param parent Pointer to parent device object for subdevice mapping support
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t power::init(zes_device_handle_t zeDevice, class device *device)
{
	zeDeviceHandle = zeDevice;
	deviceHandle = device;

	ze_result_t result = enumPowerDomains(zeDeviceHandle);
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
