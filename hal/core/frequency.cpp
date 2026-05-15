/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "frequency.h"
#include "device.h"
#include <ranges>
#include <vector>

/**
 * @brief Destructor for the frequency class
 *
 * This destructor performs cleanup operations for the frequency management
 * object, releasing allocated memory for frequency domain handles and ensuring
 * proper resource deallocation when the frequency object is destroyed.
 */
frequency::~frequency()
{
	if (frequencyHandles) {
		delete[] frequencyHandles;
		frequencyHandles = nullptr;
	}
}

/**
 * @brief Enumerates available frequency domains for a device
 *
 * This function discovers and catalogs all frequency domains available on the
 * specified device. Frequency domains represent different clock domains such as
 * GPU core, memory, and media engines that can be monitored and controlled independently.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
ze_result_t frequency::enumFrequencies(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumFrequencyDomains(device, &frequencyCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || frequencyCount == 0) {
		ERR("Failed to enumerate frequency domains or no frequency domains found. 0x{:X} ({})\n", result,
			l0_error_to_string(result));
		return result;
	}

	DBG("Found {} frequency domains.\n", frequencyCount);

	frequencyHandles = new zes_freq_handle_t[frequencyCount];
	result = zesDeviceEnumFrequencyDomains(device, &frequencyCount, frequencyHandles);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get frequency domains. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	return result;
}

/**
 * @brief Gets properties for a specific frequency domain
 *
 * This function retrieves detailed properties and characteristics for a
 * specific frequency domain, including domain type, frequency limits,
 * control capabilities, and throttling support information.
 *
 * @param frequencyHandle Handle to the specific frequency domain
 * @param properties Pointer to structure to store frequency domain properties
 * @return ze_result_t ZE_RESULT_SUCCESS on successful property retrieval, error code otherwise
 */
ze_result_t frequency::getProperties(zes_freq_handle_t frequencyHandle, zes_freq_properties_t *properties)
{
	TRACING();
	ze_result_t result = zesFrequencyGetProperties(frequencyHandle, properties);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	DBG("Frequency Properties:\n");
	DBG("  Type: ");
	switch (properties->type) {
	case ZES_FREQ_DOMAIN_GPU:
		DBG("GPU\n");
		break;
	case ZES_FREQ_DOMAIN_MEMORY:
		DBG("Memory\n");
		break;
	case ZES_FREQ_DOMAIN_MEDIA:
		DBG("Media\n");
		break;
	default:
		DBG("Other\n");
		break;
	}
	DBG("  Max Frequency: {:f} MHz\n", properties->max);
	DBG("  Min Frequency: {:f} MHz\n", properties->min);
	DBG("  Is throttle supported: {}\n", properties->isThrottleEventSupported);
	DBG("  Can control: {}\n", properties->canControl);

	return result;
}

/**
 * @brief Gets the maximum frequency for a specific frequency domain
 *
 * Scans the enumerated frequency handles to find the one matching the
 * requested domain and returns its maximum hardware frequency.
 *
 * @param domain The frequency domain to query (e.g. ZES_FREQ_DOMAIN_GPU)
 * @param maxMHz Output parameter filled with the maximum frequency in MHz
 * @return ze_result_t ZE_RESULT_SUCCESS when found, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE otherwise
 */
ze_result_t frequency::getMaxFreqForDomain(zes_freq_domain_t domain, double &maxMHz)
{
	auto handles = std::span(frequencyHandles, frequencyCount);

	// Transform handles to properties, filtering out invalid ones
	auto validProps =
		handles | std::views::transform([this](const auto &handle) -> std::optional<zes_freq_properties_t> {
			zes_freq_properties_t props{};
			props.stype = ZES_STRUCTURE_TYPE_FREQ_PROPERTIES;
			if (getProperties(handle, &props) == ZE_RESULT_SUCCESS) {
				return props;
			}
			return std::nullopt;
		}) |
		std::views::filter([](const auto &opt) { return opt.has_value(); }) |
		std::views::transform([](const auto &opt) { return *opt; }) |
		std::views::filter([domain](const auto &props) { return props.type == domain && props.max > 0.0; });

	// Find the maximum frequency

	if (auto maxIt = std::ranges::max_element(validProps, {}, &zes_freq_properties_t::max);
		maxIt != std::ranges::end(validProps)) {
		maxMHz = (*maxIt).max;
		return ZE_RESULT_SUCCESS;
	}

	return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

/**
 * @brief Gets available discrete clock frequencies for a frequency domain
 *
 * This function retrieves all available discrete clock frequencies that
 * can be set for the specified frequency domain, providing a list of
 * supported frequency values for precise clock control.
 *
 * @param frequencyHandle Handle to the specific frequency domain
 * @return ze_result_t ZE_RESULT_SUCCESS on successful clock retrieval, error code otherwise
 */
ze_result_t frequency::getAvailableClocks(zes_freq_handle_t frequencyHandle)
{
	uint32_t count = 0;
	ze_result_t result = zesFrequencyGetAvailableClocks(frequencyHandle, &count, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get available clock count. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	if (count == 0) {
		DBG("No available clocks found.\n");
		return ZE_RESULT_SUCCESS;
	}

	std::vector<double> clocks(count);
	result = zesFrequencyGetAvailableClocks(frequencyHandle, &count, clocks.data());
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get available clocks. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Available Clocks:\n");
	for (uint32_t i = 0; i < count; ++i) {
		DBG("  Clock {}: {:f} MHz\n", i, clocks[i]);
	}

	return result;
}

/**
 * @brief Gets the current frequency range settings for a frequency domain
 *
 * This function retrieves the currently configured minimum and maximum
 * frequency limits for the specified frequency domain, showing the
 * allowed operating frequency range.
 *
 * @param frequencyHandle Handle to the specific frequency domain
 * @return ze_result_t ZE_RESULT_SUCCESS on successful range retrieval, error code otherwise
 */
ze_result_t frequency::getRange(zes_freq_handle_t frequencyHandle)
{
	zes_freq_range_t range;
	ze_result_t result = zesFrequencyGetRange(frequencyHandle, &range);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get frequency range. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Frequency Range:\n");
	DBG("  Min Frequency: {:f} MHz\n", range.min);
	DBG("  Max Frequency: {:f} MHz\n", range.max);

	return result;
}

/**
 * @brief Gets the current frequency for a specific frequency domain type
 *
 * This function searches for frequency domains matching the specified type
 * and retrieves the current actual operating frequency, useful for monitoring
 * real-time clock speeds across different GPU subsystems.
 *
 * @param currentFreq Pointer to store the current frequency value (in MHz)
 * @param domain The frequency domain type to query (GPU, memory, media, etc.)
 * @return ze_result_t ZE_RESULT_SUCCESS if frequency retrieved successfully, error code otherwise
 */
ze_result_t frequency::getCurFreq(double *currentFreq, zes_freq_domain_t domain)
{
	TRACING();
	zes_freq_properties_t properties = {};
	zes_freq_state_t state = {};

	for (uint32_t i = 0; i < frequencyCount; ++i) {
		ze_result_t result = getProperties(frequencyHandles[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		if (properties.type != domain) {
			continue;
		}

		result = getState(frequencyHandles[i], &state);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		if (currentFreq) {
			*currentFreq = state.actual;
		}
		return ZE_RESULT_SUCCESS;
	}
	return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

/**
 * @brief Gets the current frequency for each tile/subdevice for a specific domain
 *
 * This function enumerates all frequency domains and returns the current frequency
 * for each subdevice/tile that matches the specified domain type (GPU or Media).
 * Used for per-tile frequency monitoring.
 *
 * @param [in] domain The frequency domain type to query (GPU, Media, etc.)
 * @param [out] tileFrequencies Map of tile_id -> current frequency in MHz
 * @return ze_result_t ZE_RESULT_SUCCESS on successful frequency retrieval
 */
ze_result_t frequency::getCurFreqPerTile(zes_freq_domain_t domain, std::map<uint32_t, double> &tileFrequencies)
{
	TRACING();
	tileFrequencies.clear();

	for (uint32_t i = 0; i < frequencyCount; ++i) {
		zes_freq_properties_t properties = {};
		ze_result_t result = getProperties(frequencyHandles[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			DBG("Failed to get properties for frequency domain {}\n", i);
			continue;
		}

		if (properties.type != domain) {
			continue;
		}

		uint32_t tileId = 0;
		if (properties.onSubdevice) {
			tileId = properties.subdeviceId;
		}

		zes_freq_state_t state = {};
		result = getState(frequencyHandles[i], &state);
		if (result != ZE_RESULT_SUCCESS) {
			DBG("Failed to get state for frequency domain {}: 0x{:X} ({})\n", i, result, l0_error_to_string(result));
			continue;
		}

		tileFrequencies[tileId] = state.actual;
		DBG("Tile {} {} frequency: {:.2f} MHz\n", tileId,
			domain == ZES_FREQ_DOMAIN_GPU ? "GPU" : (domain == ZES_FREQ_DOMAIN_MEDIA ? "Media" : "Other"),
			state.actual);
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Sets the frequency range for all frequency domains
 *
 * This function configures the minimum and maximum frequency limits for
 * all frequency domains on the device, allowing control over power consumption
 * and performance characteristics across the entire GPU.
 *
 * @param minFreq Minimum frequency limit in MHz
 * @param maxFreq Maximum frequency limit in MHz
 * @return ze_result_t ZE_RESULT_SUCCESS if frequency range set successfully, error code otherwise
 */
ze_result_t frequency::setRange(double minFreq, double maxFreq)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_freq_range_t range = {};
	range.min = minFreq;
	range.max = maxFreq;

	for (uint32_t i = 0; i < frequencyCount; ++i) {
		result = zesFrequencySetRange(frequencyHandles[i], &range);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to set frequency range. 0x{:X} ({})\n", result, l0_error_to_string(result));
			return result;
		}

		DBG("Successfully set frequency range:\n");
		DBG("  Min Frequency: {:f} MHz\n", range.min);
		DBG("  Max Frequency: {:f} MHz\n", range.max);
	}

	return result;
}

/**
 * @brief Gets the current state information for a frequency domain
 *
 * This function retrieves comprehensive frequency state information including
 * current voltage, requested frequency, actual frequency, throttle reasons,
 * and efficiency metrics for detailed performance monitoring.
 *
 * @param frequencyHandle Handle to the specific frequency domain
 * @param state Pointer to structure to store frequency state information
 * @return ze_result_t ZE_RESULT_SUCCESS on successful state retrieval, error code otherwise
 */
ze_result_t frequency::getState(zes_freq_handle_t frequencyHandle, zes_freq_state_t *state)
{
	ze_result_t result = zesFrequencyGetState(frequencyHandle, state);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get frequency state. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Frequency State:\n");
	DBG("  Current voltage: {:f} V\n", state->currentVoltage);
	DBG("  Requested Frequency: {:f} MHz\n", state->request);
	DBG("  TDP Frequency: {:f} MHz\n", state->tdp);
	DBG("  Efficient Frequency: {:f} MHz\n", state->efficient);
	DBG("  Current Frequency: {:f} MHz\n", state->actual);
	DBG("  Throttle Reasons: {}\n", state->throttleReasons);

	return result;
}

/**
 * @brief Gets accumulated throttle time for a frequency domain
 *
 * This function retrieves the total time that the frequency domain has been
 * throttled, providing insights into thermal and power management behavior
 * and system performance constraints over time.
 *
 * @param frequencyHandle Handle to the specific frequency domain
 * @return ze_result_t ZE_RESULT_SUCCESS on successful throttle time retrieval, error code otherwise
 */
ze_result_t frequency::getThrottleTime(zes_freq_handle_t frequencyHandle)
{
	zes_freq_throttle_time_t throttleTime;
	ze_result_t result = zesFrequencyGetThrottleTime(frequencyHandle, &throttleTime);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get throttle time. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Throttle Time:\n");
	DBG("  Timestamp: {} microseconds\n", throttleTime.timestamp);
	DBG("  Throttle Time: {} microseconds\n", throttleTime.throttleTime);

	return result;
}

/**
 * @brief Gets the current throttle reasons for GPU frequency domains
 *
 * This function retrieves a bitmask indicating the active throttle reasons
 * affecting GPU frequency domains, including thermal, power, current, and
 * other hardware-based throttling mechanisms for diagnostic purposes.
 *
 * @param throttleReasons Pointer to variable to store throttle reason flags bitmask
 * @return ze_result_t ZE_RESULT_SUCCESS on successful throttle reason retrieval, error code otherwise
 */
ze_result_t frequency::getThrottleReason(zes_freq_throttle_reason_flags_t *throttleReasons)
{
	TRACING();
	zes_freq_properties_t properties = {};
	zes_freq_state_t state = {};
	ze_result_t result = ZE_RESULT_SUCCESS;

	for (uint32_t i = 0; i < frequencyCount; ++i) {
		result = getProperties(frequencyHandles[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		if (properties.type == ZES_FREQ_DOMAIN_GPU) {

			result = getState(frequencyHandles[i], &state);
			if (result != ZE_RESULT_SUCCESS) {
				return result;
			}

			if (throttleReasons) {
				*throttleReasons = state.throttleReasons;
				break;
			}
		}
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Initializes the frequency management module for a device
 *
 * This function performs initial setup of frequency monitoring capabilities
 * by enumerating all available frequency domains (GPU, memory, media) on
 * the specified device for subsequent frequency operations.
 *
 * @param device Handle to the device for frequency initialization
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t frequency::init(zes_device_handle_t device)
{
	deviceHandle = device;
	parentDevice = nullptr;
	return enumFrequencies(device);
}

/**
 * @brief Initializes the frequency management module with parent device context
 *
 * @param device Handle to the device for frequency initialization
 * @param parent Pointer to parent device object for subdevice mapping support
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t frequency::init(zes_device_handle_t device, class device *parent)
{
	deviceHandle = device;
	parentDevice = parent;
	return enumFrequencies(device);
}

/**
 * @brief Performs comprehensive frequency domain runtime operations
 *
 * This function executes a complete frequency monitoring cycle including
 * property retrieval, available clocks enumeration, range configuration,
 * state monitoring, and throttle time tracking for all frequency domains.
 *
 * @param device Handle to the device (unused in current implementation)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful execution, error code otherwise
 */
ze_result_t frequency::zesRun(UNUSED zes_device_handle_t device)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_freq_properties_t properties = {};
	zes_freq_state_t state = {};

	for (uint32_t i = 0; i < frequencyCount; ++i) {
		result = getProperties(frequencyHandles[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
		result = getAvailableClocks(frequencyHandles[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
		result = getRange(frequencyHandles[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
		result = getState(frequencyHandles[i], &state);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
		result = getThrottleTime(frequencyHandles[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
	}

	return result;
}

/**
 * @brief Sets frequency range for a specific subdevice or all GPU domains
 *
 * This function configures the minimum and maximum frequency limits for either
 * a specific subdevice (when subdeviceId >= 0) or all GPU frequency domains
 * (when subdeviceId = -1). The function uses the parent device's subdevice
 * mapping to ensure proper hardware domain targeting.
 *
 * @param [in] minFreq Minimum frequency limit in MHz
 * @param [in] maxFreq Maximum frequency limit in MHz
 * @param [in] subdeviceId Subdevice identifier (-1 for all GPU domains, >= 0 for specific subdevice)
 * @retval ZE_RESULT_SUCCESS On success
 * @retval ZE_RESULT_ERROR_INVALID_ARGUMENT If subdeviceId < -1
 * @retval ZE_RESULT_ERROR_UNINITIALIZED If parent device not set for subdevice-specific operation
 * @retval ZE_RESULT_ERROR_NOT_AVAILABLE If no matching frequency domain found
 */
ze_result_t frequency::setFrequencyRange(double minFreq, double maxFreq, int32_t subdeviceId)
{
	TRACING();

	// Validate subdeviceId parameter
	if (subdeviceId < -1) {
		ERR("Invalid subdeviceId {}. Must be -1 (all) or >= 0 (specific subdevice).\n", subdeviceId);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	zes_freq_properties_t props = {};
	zes_freq_range_t range = {};
	ze_result_t result = ZE_RESULT_SUCCESS;
	bool anySuccess = false;
	bool foundDomain = false;
	ze_result_t lastError = ZE_RESULT_SUCCESS;
	bool setAll = (subdeviceId == -1);

	// Detect if device exposes subdevices
	bool hasSubdevices = false;
	uint32_t subdeviceCount = 0;
	if (deviceHandle != nullptr) {
		ze_result_t subDevRes = zesDeviceGetSubDevicePropertiesExp(deviceHandle, &subdeviceCount, nullptr);
		hasSubdevices = (subDevRes == ZE_RESULT_SUCCESS && subdeviceCount > 0);
	}

	// Get subdevice mapping if tileId is specified
	uint32_t targetSubdeviceId = 0;
	bool useDeviceLevel = false;
	if (!setAll) {
		if (!hasSubdevices) {
			if (subdeviceId != 0) {
				ERR("Invalid tileId {}. Device exposes no subdevices.\n", subdeviceId);
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}
			useDeviceLevel = true;
		} else {
			if (parentDevice == nullptr) {
				ERR("Parent device not set. Cannot map tile to subdevice.\n");
				return ZE_RESULT_ERROR_UNINITIALIZED;
			}

			zes_subdevice_exp_properties_t subdeviceProps = {};
			result = parentDevice->getSubdeviceProperties(static_cast<uint32_t>(subdeviceId), subdeviceProps);
			if (result != ZE_RESULT_SUCCESS) {
				return result;
			}
			targetSubdeviceId = subdeviceProps.subdeviceId;
		}
	}

	range.min = minFreq;
	range.max = maxFreq;

	for (uint32_t i = 0; i < frequencyCount; ++i) {
		result = getProperties(frequencyHandles[i], &props);
		if (result != ZE_RESULT_SUCCESS) {
			lastError = result;
			continue;
		}

		// Only process GPU frequency domains
		if (props.type != ZES_FREQ_DOMAIN_GPU) {
			continue;
		}

		// Check if this domain matches our criteria
		bool shouldSet = false;
		if (setAll) {
			// Set all GPU domains
			shouldSet = true;
		} else if (useDeviceLevel) {
			// Device exposes no subdevices; use device-level GPU domain
			shouldSet = (!props.onSubdevice);
		} else {
			// Set specific subdevice only - must check onSubdevice per L0 spec
			shouldSet = (props.onSubdevice && props.subdeviceId == targetSubdeviceId);
		}

		if (shouldSet) {
			foundDomain = true;
			result = zesFrequencySetRange(frequencyHandles[i], &range);
			if (result == ZE_RESULT_SUCCESS) {
				anySuccess = true;
				if (props.onSubdevice) {
					DBG("Successfully set frequency range for subdevice {}:\n", props.subdeviceId);
				} else {
					DBG("Successfully set frequency range for device-level GPU domain:\n");
				}
				DBG("  Min Frequency: {:f} MHz\n", range.min);
				DBG("  Max Frequency: {:f} MHz\n", range.max);

				// If we're setting a specific subdevice and found it, we can return
				if (!setAll) {
					return ZE_RESULT_SUCCESS;
				}
			} else {
				lastError = result;
				if (props.onSubdevice) {
					ERR("Failed to set frequency range for subdevice {}. 0x{:X} ({})\n", props.subdeviceId, result,
						l0_error_to_string(result));
				} else {
					ERR("Failed to set frequency range for device-level GPU domain. 0x{:X} ({})\n", result,
						l0_error_to_string(result));
				}
			}
		}
	}

	// If setting all, return success if any succeeded
	if (setAll) {
		return anySuccess ? ZE_RESULT_SUCCESS : lastError;
	}

	// If setting specific subdevice/device-level and we got here, determine if domain was found
	if (!foundDomain) {
		if (useDeviceLevel) {
			ERR("No matching device-level GPU frequency domain found.\n");
		} else {
			ERR("No matching GPU frequency domain found for subdevice {}\n", subdeviceId);
		}
		return ZE_RESULT_ERROR_NOT_AVAILABLE;
	}

	// Domain found but failed to set
	return lastError;
}

/**
 * @brief Sets frequency range for all GPU frequency domains
 *
 * This is a convenience wrapper around setFrequencyRange() that sets
 * the frequency range for all GPU domains on the device.
 *
 * @param [in] minFreq Minimum frequency limit in MHz
 * @param [in] maxFreq Maximum frequency limit in MHz
 * @retval ZE_RESULT_SUCCESS On success
 * @retval ZE_RESULT_ERROR_* On failure (passes through errors from setFrequencyRange)
 */
ze_result_t frequency::setFrequencyRangeForAll(double minFreq, double maxFreq)
{
	return setFrequencyRange(minFreq, maxFreq, -1);
}

/**
 * @brief Gets available clock frequencies for a specific subdevice
 *
 * This function retrieves all available discrete clock frequencies for a
 * specific subdevice's GPU frequency domain, using subdevice mapping
 * to ensure proper hardware targeting.
 *
 * @param [in] subdeviceId Subdevice identifier
 * @param [out] clocks Vector to store available clock frequencies in MHz
 * @retval ZE_RESULT_SUCCESS On success
 * @retval ZE_RESULT_ERROR_UNINITIALIZED If parent device not set
 * @retval ZE_RESULT_ERROR_NOT_AVAILABLE If no matching frequency domain found
 */
ze_result_t frequency::getFreqAvailableClocks(uint32_t subdeviceId, std::vector<double> &clocks)
{
	TRACING();
	zes_freq_properties_t props = {};
	ze_result_t result = ZE_RESULT_SUCCESS;

	// Detect if device exposes subdevices
	bool hasSubdevices = false;
	uint32_t subdeviceCount = 0;
	if (deviceHandle != nullptr) {
		ze_result_t subDevRes = zesDeviceGetSubDevicePropertiesExp(deviceHandle, &subdeviceCount, nullptr);
		hasSubdevices = (subDevRes == ZE_RESULT_SUCCESS && subdeviceCount > 0);
	}

	uint32_t targetSubdeviceId = 0;
	bool useDeviceLevel = false;
	if (!hasSubdevices) {
		if (subdeviceId != 0) {
			ERR("Invalid tileId {}. Device exposes no subdevices.\n", subdeviceId);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		useDeviceLevel = true;
	} else {
		if (parentDevice == nullptr) {
			ERR("Parent device not set. Cannot map tile to subdevice.\n");
			return ZE_RESULT_ERROR_UNINITIALIZED;
		}

		// Get subdevice mapping
		zes_subdevice_exp_properties_t subdeviceProps = {};
		result = parentDevice->getSubdeviceProperties(subdeviceId, subdeviceProps);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
		targetSubdeviceId = subdeviceProps.subdeviceId;
	}

	for (uint32_t i = 0; i < frequencyCount; ++i) {
		result = getProperties(frequencyHandles[i], &props);
		if (result != ZE_RESULT_SUCCESS) {
			continue;
		}

		// Check onSubdevice flag before accessing subdeviceId per L0 spec
		if (props.type == ZES_FREQ_DOMAIN_GPU &&
			((useDeviceLevel && !props.onSubdevice) ||
			 (!useDeviceLevel && props.onSubdevice && props.subdeviceId == targetSubdeviceId))) {
			uint32_t count = 0;
			result = zesFrequencyGetAvailableClocks(frequencyHandles[i], &count, nullptr);
			if (result != ZE_RESULT_SUCCESS) {
				ERR("Failed to get available clock count for subdevice {}. 0x{:X} ({})\n", subdeviceId, result,
					l0_error_to_string(result));
				return result;
			}

			if (count == 0) {
				DBG("No available clocks found for subdevice {}.\n", subdeviceId);
				return ZE_RESULT_SUCCESS;
			}

			clocks.resize(count);
			result = zesFrequencyGetAvailableClocks(frequencyHandles[i], &count, clocks.data());
			if (result != ZE_RESULT_SUCCESS) {
				ERR("Failed to get available clocks for subdevice {}. 0x{:X} ({})\n", subdeviceId, result,
					l0_error_to_string(result));
				return result;
			}

			DBG("Available Clocks for subdevice {}:\n", subdeviceId);
			for (uint32_t j = 0; j < count; ++j) {
				DBG("  Clock {}: {:f} MHz\n", j, clocks[j]);
			}
			return ZE_RESULT_SUCCESS;
		}
	}

	ERR("No matching GPU frequency domain found for subdevice {}\n", subdeviceId);
	return ZE_RESULT_ERROR_NOT_AVAILABLE;
}

/**
 * @brief Gets the frequency range for a specific tile
 *
 * This function retrieves the minimum and maximum GPU frequency values for a
 * specific tile, resolving tile-to-subdevice mapping and querying the appropriate
 * frequency domain handles.
 *
 * @param tileId Tile identifier to query frequency range for
 * @param minFreq Output reference for the minimum frequency in MHz
 * @param maxFreq Output reference for the maximum frequency in MHz
 * @return ze_result_t ZE_RESULT_SUCCESS on successful retrieval, error code otherwise
 */
ze_result_t frequency::getFreqRangeForTile(uint32_t tileId, double &minFreq, double &maxFreq)
{
	TRACING();
	minFreq = 0;
	maxFreq = 0;

	// Detect if device exposes subdevices
	bool hasSubdevices = false;
	uint32_t subdeviceCount = 0;
	if (deviceHandle != nullptr) {
		ze_result_t subDevRes = zesDeviceGetSubDevicePropertiesExp(deviceHandle, &subdeviceCount, nullptr);
		hasSubdevices = (subDevRes == ZE_RESULT_SUCCESS && subdeviceCount > 0);
	}

	uint32_t targetSubdeviceId = 0;
	bool useDeviceLevel = false;
	if (!hasSubdevices) {
		if (tileId != 0) {
			ERR("Invalid tileId {}. Device exposes no subdevices.\n", tileId);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		useDeviceLevel = true;
	} else {
		if (parentDevice == nullptr) {
			ERR("Parent device not set. Cannot map tile to subdevice.\n");
			return ZE_RESULT_ERROR_UNINITIALIZED;
		}

		zes_subdevice_exp_properties_t subdeviceProps = {};
		ze_result_t result = parentDevice->getSubdeviceProperties(tileId, subdeviceProps);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
		targetSubdeviceId = subdeviceProps.subdeviceId;
	}

	for (uint32_t i = 0; i < frequencyCount; ++i) {
		zes_freq_properties_t props = {};
		ze_result_t result = getProperties(frequencyHandles[i], &props);
		if (result != ZE_RESULT_SUCCESS) {
			continue;
		}

		if (props.type == ZES_FREQ_DOMAIN_GPU &&
			((useDeviceLevel && !props.onSubdevice) ||
			 (!useDeviceLevel && props.onSubdevice && props.subdeviceId == targetSubdeviceId))) {
			zes_freq_range_t range = {};
			result = zesFrequencyGetRange(frequencyHandles[i], &range);
			if (result == ZE_RESULT_SUCCESS) {
				minFreq = range.min;
				maxFreq = range.max;
			}
			return result;
		}
	}

	return ZE_RESULT_ERROR_NOT_AVAILABLE;
}

/**
 * @brief Gets the frequency state for a specific tile
 *
 * This function retrieves the current frequency state (actual, requested,
 * efficient frequencies and throttle reasons) for the GPU frequency domain
 * on a specific tile, resolving tile-to-subdevice mapping automatically.
 *
 * @param tileId Tile identifier to query frequency state for
 * @param state Pointer to the frequency state structure to populate
 * @param outProps Optional pointer to receive the frequency domain properties
 * @return ze_result_t ZE_RESULT_SUCCESS on successful retrieval, error code otherwise
 */
ze_result_t frequency::getFreqStateForTile(uint32_t tileId, zes_freq_state_t *state, zes_freq_properties_t *outProps)
{
	TRACING();

	if (state == nullptr) {
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}

	// Detect if device exposes subdevices
	bool hasSubdevices = false;
	uint32_t subdeviceCount = 0;
	if (deviceHandle != nullptr) {
		ze_result_t subDevRes = zesDeviceGetSubDevicePropertiesExp(deviceHandle, &subdeviceCount, nullptr);
		hasSubdevices = (subDevRes == ZE_RESULT_SUCCESS && subdeviceCount > 0);
	}

	uint32_t targetSubdeviceId = 0;
	bool useDeviceLevel = false;
	if (!hasSubdevices) {
		if (tileId != 0) {
			ERR("Invalid tileId {}. Device exposes no subdevices.\n", tileId);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		useDeviceLevel = true;
	} else {
		if (parentDevice == nullptr) {
			ERR("Parent device not set. Cannot map tile to subdevice.\n");
			return ZE_RESULT_ERROR_UNINITIALIZED;
		}

		zes_subdevice_exp_properties_t subdeviceProps = {};
		ze_result_t result = parentDevice->getSubdeviceProperties(tileId, subdeviceProps);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
		targetSubdeviceId = subdeviceProps.subdeviceId;
	}

	for (uint32_t i = 0; i < frequencyCount; ++i) {
		zes_freq_properties_t props = {};
		ze_result_t result = getProperties(frequencyHandles[i], &props);
		if (result != ZE_RESULT_SUCCESS) {
			continue;
		}

		if (props.type == ZES_FREQ_DOMAIN_GPU &&
			((useDeviceLevel && !props.onSubdevice) ||
			 (!useDeviceLevel && props.onSubdevice && props.subdeviceId == targetSubdeviceId))) {
			if (outProps != nullptr) {
				*outProps = props;
			}
			return getState(frequencyHandles[i], state);
		}
	}

	return ZE_RESULT_ERROR_NOT_AVAILABLE;
}

/**
 * @brief Helper function to get scheduler handle for a specific subdevice
 *
 * This function enumerates all schedulers on the device and finds the one
 * corresponding to the specified subdevice ID. It uses the parent device's
 * subdevice mapping to ensure proper hardware targeting. This is used by
 * scheduler configuration functions to avoid code duplication.
 *
 * @param [in] subdeviceId Subdevice identifier
 * @param [out] schedulerHandle Handle to the scheduler for the specified subdevice
 * @retval ZE_RESULT_SUCCESS If scheduler found
 * @retval ZE_RESULT_ERROR_INVALID_ARGUMENT If device handle is null
 * @retval ZE_RESULT_ERROR_UNINITIALIZED If parent device not set
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE If no schedulers found on device
 * @retval ZE_RESULT_ERROR_NOT_AVAILABLE If no matching scheduler found for subdevice
 */
ze_result_t frequency::getSchedulerForSubdevice(uint32_t subdeviceId, zes_sched_handle_t &schedulerHandle)
{
	if (deviceHandle == nullptr) {
		ERR("Device handle is null\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (parentDevice == nullptr) {
		ERR("Parent device not set. Cannot map tile to subdevice.\n");
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	// Get subdevice mapping using device's helper function
	zes_subdevice_exp_properties_t subdeviceProps = {};
	ze_result_t result = parentDevice->getSubdeviceProperties(subdeviceId, subdeviceProps);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}
	uint32_t targetSubdeviceId = subdeviceProps.subdeviceId;

	uint32_t schedulerCount = 0;
	result = zesDeviceEnumSchedulers(deviceHandle, &schedulerCount, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to enumerate schedulers. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	if (schedulerCount == 0) {
		ERR("No schedulers found on device\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	std::vector<zes_sched_handle_t> schedulers(schedulerCount);
	result = zesDeviceEnumSchedulers(deviceHandle, &schedulerCount, schedulers.data());
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get scheduler handles. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	// Find the scheduler for the specified subdevice
	zes_sched_handle_t deviceLevelScheduler = nullptr;
	for (const auto &sched : schedulers) {
		zes_sched_properties_t props = {};
		result = zesSchedulerGetProperties(sched, &props);
		if (result != ZE_RESULT_SUCCESS) {
			continue;
		}

		// Check onSubdevice flag before accessing subdeviceId per L0 spec
		if (props.onSubdevice && props.subdeviceId == targetSubdeviceId) {
			schedulerHandle = sched;
			DBG("Found scheduler for tile {} (subdeviceId {})\n", subdeviceId, targetSubdeviceId);
			return ZE_RESULT_SUCCESS;
		}

		// For client GPUs without per-tile schedulers, save device-level scheduler
		if (!props.onSubdevice && deviceLevelScheduler == nullptr) {
			deviceLevelScheduler = sched;
		}
	}

	// If no tile-specific scheduler found, use device-level scheduler for client GPUs
	if (deviceLevelScheduler != nullptr) {
		schedulerHandle = deviceLevelScheduler;
		DBG("Using device-level scheduler for tile {} (client GPU)\n", subdeviceId);
		return ZE_RESULT_SUCCESS;
	}

	ERR("No scheduler found for tile {} (subdeviceId {})\n", subdeviceId, targetSubdeviceId);
	return ZE_RESULT_ERROR_NOT_AVAILABLE;
}

/**
 * @brief Sets scheduler timeout mode for a specific subdevice
 *
 * This function configures the scheduler timeout mode for a specific subdevice,
 * which sets a watchdog timeout for compute operations to prevent hangs.
 *
 * @param [in] subdeviceId Subdevice identifier
 * @param [in] watchdogTimeout Watchdog timeout in microseconds
 * @retval ZE_RESULT_ERROR_INVALID_ARGUMENT If device handle is null
 * @retval ZE_RESULT_ERROR_UNINITIALIZED If parent device not set
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE If no schedulers found on device
 * @retval ZE_RESULT_ERROR_NOT_AVAILABLE If no matching scheduler found for subdevice
 */
ze_result_t frequency::setSchedulerTimeoutMode(uint32_t subdeviceId, uint64_t watchdogTimeout)
{
	TRACING();

	zes_sched_handle_t scheduler;
	ze_result_t result = getSchedulerForSubdevice(subdeviceId, scheduler);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	ze_bool_t needReload = false;
	zes_sched_timeout_properties_t timeoutProps = {};
	timeoutProps.stype = ZES_STRUCTURE_TYPE_SCHED_TIMEOUT_PROPERTIES;
	timeoutProps.pNext = nullptr;
	timeoutProps.watchdogTimeout = watchdogTimeout;

	result = zesSchedulerSetTimeoutMode(scheduler, &timeoutProps, &needReload);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to set scheduler timeout mode for subdevice {}. 0x{:X} ({})\n", subdeviceId, result,
			l0_error_to_string(result));
		return result;
	}

	DBG("Successfully set scheduler timeout mode for subdevice {}\n", subdeviceId);
	DBG("  Watchdog Timeout: {} microseconds\n", watchdogTimeout);
	DBG("  Need Reload: {}\n", needReload ? "true" : "false");

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Sets scheduler timeslice mode for a specific subdevice
 *
 * This function configures the scheduler timeslice mode for a specific subdevice,
 * controlling how compute workloads are time-multiplexed on the hardware.
 *
 * @param [in] subdeviceId Subdevice identifier
 * @param [in] interval Timeslice interval in microseconds
 * @param [in] yieldTimeout Yield timeout in microseconds
 * @retval ZE_RESULT_SUCCESS On success
 * @retval ZE_RESULT_ERROR_INVALID_ARGUMENT If device handle is null
 * @retval ZE_RESULT_ERROR_UNINITIALIZED If parent device not set
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE If no schedulers found on device
 * @retval ZE_RESULT_ERROR_NOT_AVAILABLE If no matching scheduler found for subdevice
 */
ze_result_t frequency::setSchedulerTimesliceMode(uint32_t subdeviceId, uint64_t interval, uint64_t yieldTimeout)
{
	TRACING();

	zes_sched_handle_t scheduler;
	ze_result_t result = getSchedulerForSubdevice(subdeviceId, scheduler);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	ze_bool_t needReload = false;
	zes_sched_timeslice_properties_t timesliceProps = {};
	timesliceProps.stype = ZES_STRUCTURE_TYPE_SCHED_TIMESLICE_PROPERTIES;
	timesliceProps.pNext = nullptr;
	timesliceProps.interval = interval;
	timesliceProps.yieldTimeout = yieldTimeout;

	result = zesSchedulerSetTimesliceMode(scheduler, &timesliceProps, &needReload);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to set scheduler timeslice mode for subdevice {}. 0x{:X} ({})\n", subdeviceId, result,
			l0_error_to_string(result));
		return result;
	}

	DBG("Successfully set scheduler timeslice mode for subdevice {}\n", subdeviceId);
	DBG("  Interval: {} microseconds\n", interval);
	DBG("  Yield Timeout: {} microseconds\n", yieldTimeout);
	DBG("  Need Reload: {}\n", needReload ? "true" : "false");

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Sets scheduler exclusive mode for a specific subdevice
 *
 * This function configures the scheduler to exclusive mode for a specific subdevice,
 * where a single process gets dedicated access to the compute resources.
 *
 * @param [in] subdeviceId Subdevice identifier
 * @retval ZE_RESULT_ERROR_INVALID_ARGUMENT If device handle is null
 * @retval ZE_RESULT_ERROR_UNINITIALIZED If parent device not set
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE If no schedulers found on device
 * @retval ZE_RESULT_ERROR_NOT_AVAILABLE If no matching scheduler found for subdevice
 */
ze_result_t frequency::setSchedulerExclusiveMode(uint32_t subdeviceId)
{
	TRACING();

	zes_sched_handle_t scheduler;
	ze_result_t result = getSchedulerForSubdevice(subdeviceId, scheduler);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	ze_bool_t needReload = false;
	result = zesSchedulerSetExclusiveMode(scheduler, &needReload);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to set scheduler exclusive mode for subdevice {}. 0x{:X} ({})\n", subdeviceId, result,
			l0_error_to_string(result));
		return result;
	}

	DBG("Successfully set scheduler exclusive mode for subdevice {}\n", subdeviceId);
	DBG("  Need Reload: {}\n", needReload ? "true" : "false");

	return ZE_RESULT_SUCCESS;
}