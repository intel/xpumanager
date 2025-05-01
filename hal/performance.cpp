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

performance::~performance()
{
	if (perfHandles)
	{
		delete[] perfHandles;
		perfHandles = nullptr;
	}
}

ze_result_t performance::enumPerformanceFactorDomains(zes_device_handle_t device)
{
	// Get the perfCount of performance factor domains
	ze_result_t result = zesDeviceEnumPerformanceFactorDomains(device, &perfCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || perfCount == 0)
	{
		ERR("Failed to get performance factor domains perfCount or no domains available. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	perfHandles = new zes_perf_handle_t[perfCount];

	// Retrieve the performance factor domain handles
	result = zesDeviceEnumPerformanceFactorDomains(device, &perfCount, perfHandles);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to enumerate performance factor domains. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Found %d performance factor domains.\n", perfCount);
	return result;
}

ze_result_t performance::getProperties(zes_perf_handle_t perfHandle)
{
	zes_perf_properties_t properties = {};
	ze_result_t result = zesPerformanceFactorGetProperties(perfHandle, &properties);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get properties for performance factor domain 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Performance Factor Properties:\n");
	DBG("  onSubdevice: %d\n", properties.onSubdevice);
	DBG("  subdeviceId: %d\n", properties.subdeviceId);
	printEngines(properties.engines);

	return result;
}

ze_result_t performance::getConfig(zes_perf_handle_t perfHandle)
{
	double factor = 0.0;
	ze_result_t result = zesPerformanceFactorGetConfig(perfHandle, &factor);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get config for performance factor domain 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Performance Factor Config:\n");
	DBG("  Factor: %f\n", factor);

	return result;
}

ze_result_t performance::zesRun(zes_device_handle_t device)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	enumPerformanceFactorDomains(device);

	for (uint32_t i = 0; i < perfCount; ++i)
	{
		result = getProperties(perfHandles[i]);
		if (result != ZE_RESULT_SUCCESS)
		{
			return result;
		}

		result = getConfig(perfHandles[i]);
		if (result != ZE_RESULT_SUCCESS)
		{
			return result;
		}
	}
	return result;
}