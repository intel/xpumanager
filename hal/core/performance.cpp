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
#include "performance.h"

/**
 * @brief Enumerates available performance factor domains for a device
 *
 * This function discovers and catalogs all performance factor domains available
 * on the specified device. Performance domains represent different execution
 * engines (compute, media, etc.) that can have their performance factors tuned.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
ze_result_t performance::enumPerformanceFactorDomains(zes_device_handle_t device)
{
	uint32_t perfCount = 0;
	// Get the perfCount of performance factor domains
	ze_result_t result = zesDeviceEnumPerformanceFactorDomains(device, &perfCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || perfCount == 0) {
		ERR("Failed to get performance factor domains perfCount or no domains available. 0x%X (%s)\n", result,
			l0_error_to_string(result));
		return result;
	}

	perfHandles->clear();
	perfHandles->resize(perfCount);

	// Retrieve the performance factor domain handles
	result = zesDeviceEnumPerformanceFactorDomains(device, &perfCount, perfHandles->data());
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to enumerate performance factor domains. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Found %d performance factor domains.\n", perfCount);
	return result;
}

/**
 * @brief Gets properties for a specific performance factor domain
 *
 * This function retrieves detailed properties and characteristics for a
 * specific performance factor domain, including supported engines and
 * subdevice association for performance tuning configuration.
 *
 * @param perfHandle Handle to the specific performance factor domain
 * @return ze_result_t ZE_RESULT_SUCCESS on successful property retrieval, error code otherwise
 */
ze_result_t performance::getProperties(zes_perf_handle_t perfHandle, zes_perf_properties_t *properties)
{
	TRACING();

	if (properties == nullptr) {
		ERR("Invalid properties pointer provided.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	ze_result_t result = zesPerformanceFactorGetProperties(perfHandle, properties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get properties for performance factor domain 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Performance Factor Properties:\n");
	DBG("  onSubdevice: %d\n", properties->onSubdevice);
	DBG("  subdeviceId: %d\n", properties->subdeviceId);
	printEngines(properties->engines);

	return result;
}

/**
 * @brief Gets the current configuration for a performance factor domain
 *
 * This function retrieves the current performance factor configuration for a
 * specific domain, returning the performance scaling factor that affects
 * execution engine performance characteristics and power consumption.
 *
 * @param perfHandle Handle to the specific performance factor domain
 * @return ze_result_t ZE_RESULT_SUCCESS on successful configuration retrieval, error code otherwise
 */
ze_result_t performance::getConfig(zes_perf_handle_t perfHandle)
{
	double factor = 0.0;
	ze_result_t result = zesPerformanceFactorGetConfig(perfHandle, &factor);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get config for performance factor domain 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Performance Factor Config:\n");
	DBG("  Factor: %f\n", factor);

	return result;
}

/**
 * @brief Sets the performance factor configuration for a specific engine type
 *
 * This function configures the performance factor for performance domains that match
 * the specified engine type. It validates the factor value and applies the configuration
 * to all matching performance factor domains on the device.
 *
 * @param engineType The engine type flag specifying which execution engines to configure
 * @param factor The performance factor value (0-100) to set for the matching domains
 * @return ze_result_t ZE_RESULT_SUCCESS on successful configuration, error code otherwise
 */
ze_result_t performance::setConfig(zes_engine_type_flag_t engineType, double factor)
{
	TRACING();

	// Assume that this feature is not supported
	ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

	// Validate the factor value
	if (factor < 0 || factor > 100) {
		ERR("Invalid performance factor value: %f. Must be between 0 and 100.\n", factor);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	for (uint32_t i = 0; i < perfHandles->size(); ++i) {
		zes_perf_properties_t properties = {};

		result = getProperties(perfHandles->at(i), &properties);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		// Check if the engine type matches the properties of the performance factor domain
		if (properties.engines & engineType) {

			DBG("Setting config for performance factor domain %d with factor %f\n", i, factor);
			// Set the performance factor configuration
			result = zesPerformanceFactorSetConfig(perfHandles->at(i), factor);
			if (result != ZE_RESULT_SUCCESS) {
				ERR("Failed to set config for performance factor domain 0x%X (%s)\n", result,
					l0_error_to_string(result));
				return result;
			}
		}
	}
	return result;
}

/**
 * @brief Initializes the performance monitoring subsystem for a device
 *
 * This function performs initialization of the performance monitoring capabilities
 * by enumerating all available performance factor domains on the specified device.
 * This is typically called during device setup to prepare performance tuning functionality.
 *
 * @param device Handle to the Level Zero Sysman device to initialize
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t performance::init(zes_device_handle_t device)
{
	TRACING();
	return enumPerformanceFactorDomains(device);
}

/**
 * @brief Performs comprehensive performance factor monitoring runtime operations
 *
 * This function executes a complete performance factor monitoring cycle including
 * domain enumeration, property retrieval, and configuration checking for all
 * performance domains to provide comprehensive performance tuning analysis.
 *
 * @param device Handle to the device for performance factor operations
 * @return ze_result_t ZE_RESULT_SUCCESS on successful execution, error code otherwise
 */
ze_result_t performance::zesRun(UNUSED zes_device_handle_t device)
{
	ze_result_t result = ZE_RESULT_SUCCESS;

	for (uint32_t i = 0; i < perfHandles->size(); ++i) {
		zes_perf_properties_t properties = {};

		result = getProperties(perfHandles->at(i), &properties);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = getConfig(perfHandles->at(i));
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
	}
	return result;
}
