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
#include <vector>
#include <loader/ze_loader.h>
#include "driver.h"

using namespace std;

ze_result_t driver::init()
{
	// Initialize the Level Zero API
	ze_result_t result = zeInit(ZE_INIT_FLAG_GPU_ONLY);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to initialize Level Zero. Error code: %d\n", result);
		return result;
	}

	DBG("ZE API initialized successfully.\n");

	// Initialize the Level Zero System Management API
	result = zesInit(ZE_INIT_FLAG_GPU_ONLY);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to initialize ZES API: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("ZES API initialized successfully.\n");

	// Discover the number of driver instances
	result = zesDriverGet(&driverCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || driverCount == 0)
	{
		ERR("Failed to get driver count: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Number of zesDrivers: %d\n", driverCount);

	zeDrivers = new ze_driver_handle_t[driverCount];
	if (zeDrivers == nullptr)
	{
		ERR("Failed to allocate memory for driver handles.\n");
		return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
	}

	// Retrieve driver handles
	result = zeDriverGet(&driverCount, zeDrivers);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get driver handles: 0x%X (%s)\n", result, l0_error_to_string(result));
		delete[] zeDrivers;
		return result;
	}

	zesDrivers = new ze_driver_handle_t[driverCount];
	if (zesDrivers == nullptr)
	{
		ERR("Failed to allocate memory for driver handles.\n");
		return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
	}

	// Retrieve driver handles
	result = zesDriverGet(&driverCount, zesDrivers);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get driver handles: 0x%X (%s)\n", result, l0_error_to_string(result));
		delete[] zesDrivers;
		return result;
	}

	DBG("Driver handles retrieved successfully.\n");

	devs = new device[driverCount];

	for (uint32_t i = 0; i < driverCount; i++)
	{
		ze_api_version_t apiVersion = {};
		result = zeDriverGetApiVersion(zeDrivers[i], &apiVersion);
		if (result == ZE_RESULT_SUCCESS)
		{
#define PRINT_API_VERSION(i, x)                    \
	case x:                                        \
		DBG("Driver %u API Version: %s\n", i, #x); \
		break;

			switch (apiVersion)
			{
				PRINT_API_VERSION(i, ZE_API_VERSION_1_0);
				PRINT_API_VERSION(i, ZE_API_VERSION_1_1);
				PRINT_API_VERSION(i, ZE_API_VERSION_1_2);
				PRINT_API_VERSION(i, ZE_API_VERSION_1_3);
				PRINT_API_VERSION(i, ZE_API_VERSION_1_4);
				PRINT_API_VERSION(i, ZE_API_VERSION_1_5);
				PRINT_API_VERSION(i, ZE_API_VERSION_1_6);
				PRINT_API_VERSION(i, ZE_API_VERSION_1_7);
				PRINT_API_VERSION(i, ZE_API_VERSION_1_8);
				PRINT_API_VERSION(i, ZE_API_VERSION_1_9);
				PRINT_API_VERSION(i, ZE_API_VERSION_1_10);
				PRINT_API_VERSION(i, ZE_API_VERSION_1_11);
				PRINT_API_VERSION(i, ZE_API_VERSION_1_12);
			default:
				DBG("Unknown version\n");
				break;
			}
		}
		else
		{
			ERR("Failed to get API version for driver %u. Error code: %d\n", i, result);
			delete[] zeDrivers;
			delete[] zesDrivers;
			delete[] devs;
			return result;
		}

		PRINT("\n==============================================\n");

		result = devs[i].init(zeDrivers[i], zesDrivers[i]);
		if (result != ZE_RESULT_SUCCESS)
		{
			ERR("Failed to initialize device for driver %u. Error code: %d\n", i, result);
			delete[] zeDrivers;
			delete[] zesDrivers;
			delete[] devs;
			return result;
		}
	}
	initialized = true;
	return ZE_RESULT_SUCCESS;
}

driver::~driver()
{
	if (devs != nullptr)
	{
		delete[] devs;
		devs = nullptr;
	}
	// Clean up the ZE driver handles
	if (zeDrivers != nullptr)
	{
		delete[] zeDrivers;
		zeDrivers = nullptr;
	}
	// Clean up the ZES driver handles
	if (zesDrivers != nullptr)
	{
		delete[] zesDrivers;
		zesDrivers = nullptr;
	}
}

ze_result_t driver::getExtensionProperties(ze_driver_handle_t driver)
{
	uint32_t extensionCount = 0;
	ze_result_t result = zeDriverGetExtensionProperties(driver, &extensionCount, NULL);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get extension count. Error code: %d\n", result);
		return result;
	}

	if (extensionCount == 0)
	{
		DBG("No extensions available for this driver.\n");
		return ZE_RESULT_SUCCESS;
	}

	ze_driver_extension_properties_t *extensions = new ze_driver_extension_properties_t[extensionCount];
	if (extensions == NULL)
	{
		ERR("Failed to allocate memory for extension properties.\n");
		return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
	}

	result = zeDriverGetExtensionProperties(driver, &extensionCount, extensions);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get extension properties. Error code: %d\n", result);
		delete[] extensions;
		return result;
	}

	DBG("Extension Properties:\n");
	for (uint32_t i = 0; i < extensionCount; ++i)
	{
		DBG("  Extension %u: %s, version 0x%X\n", i + 1, extensions[i].name, extensions[i].version);
	}

	delete[] extensions;
	return ZE_RESULT_SUCCESS;
}

ze_result_t driver::getDriverProperties(ze_driver_handle_t driver)
{
	ze_driver_properties_t driverProperties;
	ze_result_t result = zeDriverGetProperties(driver, &driverProperties);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get driver properties. Error code: %d\n", result);
		return result;
	}

	DBG("Driver Properties:\n");
	DBG("  UUID: ");
	for (int j = 0; j < ZE_MAX_DRIVER_UUID_SIZE; ++j)
	{
		DBG("%02x", driverProperties.uuid.id[j]);
	}
	DBG("\n");
	DBG("  Driver Version: 0x%X\n", driverProperties.driverVersion);
	return result;
}

ze_result_t driver::getIpcProperties(ze_driver_handle_t driver)
{
	ze_driver_ipc_properties_t ipcProperties;
	ze_result_t result = zeDriverGetIpcProperties(driver, &ipcProperties);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get IPC properties. Error code: %d\n", result);
		return result;
	}

	// If successful, print the IPC properties
	DBG("IPC Properties:\n");
	DBG("  Supports passing memory allocations between processes: %s\n",
		ipcProperties.flags & ZE_IPC_PROPERTY_FLAG_MEMORY ? "Yes" : "No");
	DBG("  Supports passing event pools between processes: %s\n",
		ipcProperties.flags & ZE_IPC_PROPERTY_FLAG_EVENT_POOL ? "Yes" : "No");

	return ZE_RESULT_SUCCESS;
}

ze_result_t driver::run()
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	// Check if the driver has been initialized
	if (!initialized)
	{
		ERR("Driver not initialized. Call init() first.\n");
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	// Iterate over each driver and get extension properties
	for (uint32_t i = 0; i < driverCount; i++)
	{
		getDriverProperties(zeDrivers[i]);
		getIpcProperties(zeDrivers[i]);
		getExtensionProperties(zeDrivers[i]);

		// Run device operations
		result = devs[i].run();
		if (result != ZE_RESULT_SUCCESS)
		{
			ERR("Failed to run device operations: 0x%X (%s)\n", result, l0_error_to_string(result));
		}
	}
	return result;
}

void driver::printLoaderVersions()
{
	zel_component_version_t *versions;
	size_t size = 0;
	zelLoaderGetVersions(&size, nullptr);
	DBG("zelLoaderGetVersions number of components found: %zd\n", size);
	versions = new zel_component_version_t[size];
	zelLoaderGetVersions(&size, versions);

	for (size_t i = 0; i < size; i++)
	{
		PRINT("Level Zero Version: %d.%d.%d\n",
			  versions[i].component_lib_version.major,
			  versions[i].component_lib_version.minor,
			  versions[i].component_lib_version.patch);
	}

	delete[] versions;
}

ze_device_handle_t driver::findDeviceByBDF(const char *bdf)
{
	ze_device_handle_t foundDevice = nullptr;

	for (uint32_t i = 0; i < driverCount; i++)
	{
		foundDevice = devs[i].findDeviceByBDF(bdf);
		if (foundDevice != nullptr)
		{
			DBG("Found device with BDF: %s\n", bdf);
			break;
		}
	}

	if (foundDevice == nullptr)
	{
		DBG("No device found with BDF: %s\n", bdf);
	}
	return foundDevice;
}

ze_device_handle_t driver::findDeviceByIndex(uint32_t index)
{
	ze_device_handle_t foundDevice = nullptr;

	for (uint32_t i = 0; i < driverCount; i++)
	{
		foundDevice = devs[i].findDeviceByIndex(index);
		if (foundDevice != nullptr)
		{
			DBG("Found device with index: %u\n", index);
			break;
		}
	}

	if (foundDevice == nullptr)
	{
		DBG("No device found with index: %u\n", index);
	}
	return foundDevice;
}
