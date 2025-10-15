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

/**
 * @brief Destructor for the temperature class
 *
 * This destructor performs cleanup operations for the temperature management
 * object, releasing allocated memory for temperature sensor handles and ensuring
 * proper resource deallocation when the temperature object is destroyed.
 */
temperature::~temperature()
{
	if (temperatureHandles) {
		delete[] temperatureHandles;
		temperatureHandles = nullptr;
	}
}

/**
 * @brief Enumerates available temperature sensor domains for a device
 *
 * This function discovers and catalogs all temperature sensors available on the
 * specified device. Temperature sensors monitor thermal conditions across
 * different components and regions of the device.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
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

/**
 * @brief Gets properties for a specific temperature sensor
 *
 * This function retrieves detailed properties and capabilities for a
 * specific temperature sensor, including sensor type and critical thresholds.
 *
 * @param temperatureHandle Handle to the specific temperature sensor
 * @param properties Pointer to structure to store temperature sensor properties
 * @return ze_result_t ZE_RESULT_SUCCESS on successful property retrieval, error code otherwise
 */
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

/**
 * @brief Gets the current temperature state for a specific sensor
 *
 * This function retrieves the current temperature reading from a specific
 * temperature sensor, providing real-time thermal monitoring capabilities
 * for device safety and performance optimization.
 *
 * @param temperatureHandle Handle to the specific temperature sensor
 * @param temp Pointer to store the current temperature value in Celsius
 * @return ze_result_t ZE_RESULT_SUCCESS on successful temperature retrieval, error code otherwise
 */
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

/**
 * @brief Gets temperature for a specific sensor type
 *
 * This function searches for temperature sensors matching the specified type
 * and retrieves the current temperature reading, providing targeted thermal
 * monitoring for specific device components.
 *
 * @param type The specific temperature sensor type to query
 * @param coreTemp Pointer to store the temperature value in Celsius
 * @return ze_result_t ZE_RESULT_SUCCESS if temperature retrieved successfully, error code otherwise
 */
ze_result_t temperature::getTemp(zes_temp_sensors_t type, double *temp)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_temp_properties_t properties;
	*temp = 0.0; // Default value if no temperature found

	for (uint32_t i = 0; i < temperatureCount; ++i) {
		result = getProperties(temperatureHandles[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		if (properties.type == type) {
			result = getState(temperatureHandles[i], temp);
			return result;
		}
	}
	return result;
}

/**
 * @brief Gets the current GPU core temperature
 *
 * This function retrieves the current temperature reading from the GPU core
 * temperature sensor, providing essential thermal monitoring for GPU performance
 * and safety management.
 *
 * @param coreTemp Pointer to store the GPU core temperature in Celsius
 * @return ze_result_t ZE_RESULT_SUCCESS if core temperature retrieved successfully, error code otherwise
 */
ze_result_t temperature::getCoreTemp(double *coreTemp)
{
	TRACING();
	return getTemp(ZES_TEMP_SENSORS_GPU, coreTemp);
}

/**
 * @brief Gets the current memory temperature
 *
 * This function retrieves the current temperature reading from the memory
 * temperature sensor, providing thermal monitoring capabilities for memory
 * subsystem safety and performance optimization.
 *
 * @param memTemp Pointer to store the memory temperature in Celsius
 * @return ze_result_t ZE_RESULT_SUCCESS if memory temperature retrieved successfully, error code otherwise
 */
ze_result_t temperature::getMemoryTemp(double *memTemp)
{
	TRACING();
	return getTemp(ZES_TEMP_SENSORS_MEMORY, memTemp);
}

/**
 * @brief Gets GPU core temperature threshold values
 *
 * This function retrieves the thermal threshold values for GPU core components,
 * including throttle and shutdown temperature limits used for thermal protection
 * and performance management.
 *
 * @param device Handle to the device (unused in current implementation)
 * @param throttleThreshold Pointer to store the throttle temperature threshold
 * @param shutdownThreshold Pointer to store the shutdown temperature threshold
 * @return ze_result_t ZE_RESULT_SUCCESS on successful threshold retrieval, error code otherwise
 */
ze_result_t temperature::getCoreThreshold(UNUSED zes_device_handle_t device, uint32_t *throttleThreshold,
										  uint32_t *shutdownThreshold)
{
	TRACING();

	ze_result_t result = ZE_RESULT_SUCCESS;

	*throttleThreshold = CORE_THROTTLE_THRESHOLD_DEFAULT;
	*shutdownThreshold = CORE_SHUTDOWN_THRESHOLD_DEFAULT;
	return result;
}

/**
 * @brief Gets memory temperature threshold values
 *
 * This function retrieves the thermal threshold values for memory components,
 * including throttle and shutdown temperature limits used for memory thermal
 * protection and system stability management.
 *
 * @param device Handle to the device (unused in current implementation)
 * @param throttleThreshold Pointer to store the memory throttle temperature threshold
 * @param shutdownThreshold Pointer to store the memory shutdown temperature threshold
 * @return ze_result_t ZE_RESULT_SUCCESS on successful threshold retrieval, error code otherwise
 */
ze_result_t temperature::getMemoryThreshold(UNUSED zes_device_handle_t device, uint32_t *throttleThreshold,
											uint32_t *shutdownThreshold)
{
	TRACING();

	ze_result_t result = ZE_RESULT_SUCCESS;

	*throttleThreshold = MEMORY_THROTTLE_THRESHOLD_DEFAULT;
	*shutdownThreshold = MEMORY_SHUTDOWN_THRESHOLD_DEFAULT;
	return result;
}

/**
 * @brief Initializes the temperature management module for a device
 *
 * This function performs initial setup of temperature monitoring capabilities
 * by enumerating all available temperature sensors on the specified device
 * for subsequent thermal monitoring operations.
 *
 * @param device Handle to the device for temperature initialization
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t temperature::init(zes_device_handle_t device)
{
	TRACING();
	return enumTemperatureDomains(device);
}

/**
 * @brief Performs comprehensive temperature monitoring runtime operations
 *
 * This function executes a complete temperature monitoring cycle for all
 * temperature sensors, including property retrieval and current temperature
 * readings for comprehensive thermal analysis and system health monitoring.
 *
 * @param device Handle to the device (unused in current implementation)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful execution, error code otherwise
 */
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
