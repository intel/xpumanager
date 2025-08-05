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
#include "frequency.h"

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
		ERR("Failed to enumerate frequency domains or no frequency domains found. 0x%X (%s)\n", result,
			l0_error_to_string(result));
		return result;
	}

	DBG("Found %d frequency domains.\n", frequencyCount);

	frequencyHandles = new zes_freq_handle_t[frequencyCount];
	result = zesDeviceEnumFrequencyDomains(device, &frequencyCount, frequencyHandles);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get frequency domains. 0x%X (%s)\n", result, l0_error_to_string(result));
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
	DBG("  Max Frequency: %f MHz\n", properties->max);
	DBG("  Min Frequency: %f MHz\n", properties->min);
	DBG("  Is throttle supported: %d\n", properties->isThrottleEventSupported);
	DBG("  Can control: %d\n", properties->canControl);

	return result;
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
		ERR("Failed to get available clock count. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	if (count == 0) {
		DBG("No available clocks found.\n");
		return ZE_RESULT_SUCCESS;
	}

	double *clocks = new double[count];
	result = zesFrequencyGetAvailableClocks(frequencyHandle, &count, clocks);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get available clocks. 0x%X (%s)\n", result, l0_error_to_string(result));
		delete[] clocks;
		return result;
	}

	DBG("Available Clocks:\n");
	for (uint32_t i = 0; i < count; ++i) {
		DBG("  Clock %u: %f MHz\n", i, clocks[i]);
	}

	delete[] clocks;
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
		ERR("Failed to get frequency range. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Frequency Range:\n");
	DBG("  Min Frequency: %f MHz\n", range.min);
	DBG("  Max Frequency: %f MHz\n", range.max);

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
	ze_result_t result = ZE_RESULT_SUCCESS;

	for (uint32_t i = 0; i < frequencyCount; ++i) {
		result = getProperties(frequencyHandles[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = getState(frequencyHandles[i], &state);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		if (properties.type == domain) {
			if (currentFreq) {
				*currentFreq = state.actual;
				break;
			}
		}
	}
	return result;
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
			ERR("Failed to set frequency range. 0x%X (%s)\n", result, l0_error_to_string(result));
			return result;
		}

		DBG("Successfully set frequency range:\n");
		DBG("  Min Frequency: %f MHz\n", range.min);
		DBG("  Max Frequency: %f MHz\n", range.max);
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
		ERR("Failed to get frequency state. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Frequency State:\n");
	DBG("  Current voltage: %f MHz\n", state->currentVoltage);
	DBG("  Requested Frequency: %f MHz\n", state->request);
	DBG("  TDP Frequency: %f MHz\n", state->tdp);
	DBG("  Efficient Frequency: %f MHz\n", state->efficient);
	DBG("  Current Frequency: %f MHz\n", state->actual);
	DBG("  Throttle Reasons: %u\n", state->throttleReasons);

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
		ERR("Failed to get throttle time. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Throttle Time:\n");
	DBG("  Timestamp: %" PRIu64 "\n", throttleTime.timestamp);
	DBG("  Throttle Time: %" PRIu64 "microseconds\n", throttleTime.throttleTime);

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
ze_result_t frequency::init(zes_device_handle_t device) { return enumFrequencies(device); }

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
