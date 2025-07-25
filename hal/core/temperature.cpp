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
#include "temperature.h"

temperature::~temperature()
{
	if (temperatureHandles) {
		delete[] temperatureHandles;
		temperatureHandles = nullptr;
	}
}

ze_result_t temperature::enumTemperatureDomains(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumTemperatureSensors(device, &temperatureCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || temperatureCount == 0) {
		ERR("Failed to enumerate temperature domains. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	temperatureHandles = new zes_temp_handle_t[temperatureCount];
	result = zesDeviceEnumTemperatureSensors(device, &temperatureCount, temperatureHandles);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get temperature domains. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Found %d temperature domains.\n", temperatureCount);
	return result;
}

ze_result_t temperature::getProperties(zes_temp_handle_t temperatureHandle, zes_temp_properties_t *properties)
{
	ze_result_t result = zesTemperatureGetProperties(temperatureHandle, properties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get properties for temperature domain 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Temperature Properties:\n");
	DBG("  onSubdevice: %d\n", properties->onSubdevice);
	DBG("  subdeviceId: %d\n", properties->subdeviceId);
	switch (properties->type) {
	case ZES_TEMP_SENSORS_GLOBAL:
		DBG("  type: Global\n");
		break;
	case ZES_TEMP_SENSORS_GPU:
		DBG("  type: GPU\n");
		break;
	case ZES_TEMP_SENSORS_MEMORY:
		DBG("  type: Memory\n");
		break;
	case ZES_TEMP_SENSORS_GLOBAL_MIN:
		DBG("  type: Global Min\n");
		break;
	case ZES_TEMP_SENSORS_GPU_MIN:
		DBG("  type: GPU Min\n");
		break;
	case ZES_TEMP_SENSORS_MEMORY_MIN:
		DBG("  type: Memory Min\n");
		break;
	case ZES_TEMP_SENSORS_GPU_BOARD:
		DBG("  type: GPU Board\n");
		break;
	case ZES_TEMP_SENSORS_GPU_BOARD_MIN:
		DBG("  type: GPU Board Min\n");
		break;
	default:
		DBG("  type: Unknown (%d)\n", properties->type);
		break;
	}
	DBG("  maxTemperature: %f C\n", properties->maxTemperature);
	DBG("  isCriticalTempSupported: %d\n", properties->isCriticalTempSupported);
	DBG("  isThreshold1Supported: %d\n", properties->isThreshold1Supported);
	DBG("  isThreshold2Supported: %d\n", properties->isThreshold2Supported);

	return result;
}

ze_result_t temperature::getState(zes_temp_handle_t temperatureHandle, double *temp)
{
	ze_result_t result = zesTemperatureGetState(temperatureHandle, temp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get state for temperature domain 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Temperature State:\n");
	DBG("  Current Temperature: %f C\n", *temp);

	return result;
}

ze_result_t temperature::getTemp(UNUSED zes_device_handle_t device, zes_temp_sensors_t type, double *coreTemp)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_temp_properties_t properties;
	*coreTemp = 0.0; // Default value if no temperature found

	for (uint32_t i = 0; i < temperatureCount; ++i) {
		result = getProperties(temperatureHandles[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		if (properties.type == type) {
			result = getState(temperatureHandles[i], coreTemp);
			return result;
		}
	}
	return result;
}

ze_result_t temperature::getCoreTemp(zes_device_handle_t device, double *coreTemp)
{
	TRACING();
	return getTemp(device, ZES_TEMP_SENSORS_GPU, coreTemp);
}

ze_result_t temperature::getMemoryTemp(zes_device_handle_t device, double *coreTemp)
{
	TRACING();
	return getTemp(device, ZES_TEMP_SENSORS_MEMORY, coreTemp);
}

ze_result_t temperature::getCoreThreshold(UNUSED zes_device_handle_t device, uint32_t *throttleThreshold,
										  uint32_t *shutdownThreshold)
{
	TRACING();

	ze_result_t result = ZE_RESULT_SUCCESS;

	*throttleThreshold = CORE_THROTTLE_THRESHOLD_DEFAULT;
	*shutdownThreshold = CORE_SHUTDOWN_THRESHOLD_DEFAULT;
	return result;
}

ze_result_t temperature::getMemoryThreshold(UNUSED zes_device_handle_t device, uint32_t *throttleThreshold,
											uint32_t *shutdownThreshold)
{
	TRACING();

	ze_result_t result = ZE_RESULT_SUCCESS;

	*throttleThreshold = MEMORY_THROTTLE_THRESHOLD_DEFAULT;
	*shutdownThreshold = MEMORY_SHUTDOWN_THRESHOLD_DEFAULT;
	return result;
}

ze_result_t temperature::init(zes_device_handle_t device)
{
	TRACING();
	return enumTemperatureDomains(device);
}

ze_result_t temperature::zesRun(UNUSED zes_device_handle_t device)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_temp_properties_t properties;
	double temp;

	for (uint32_t i = 0; i < temperatureCount; ++i) {
		result = getProperties(temperatureHandles[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = getState(temperatureHandles[i], &temp);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
	}

	return result;
}
