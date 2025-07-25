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

frequency::~frequency()
{
	if (frequencyHandles) {
		delete[] frequencyHandles;
		frequencyHandles = nullptr;
	}
}

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

ze_result_t frequency::init(zes_device_handle_t device) { return enumFrequencies(device); }

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
