/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "driver.h"
#include "driver_util.h"
#include <loader/ze_loader.h>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Sets the print/debug level for the driver and synchronizes across modules
 *
 * This function ensures that debug level changes are properly synchronized between
 * different compilation units (CLI and library) when using separate builds like meson.
 *
 * @param lvl Debug level to set (e.g., DBG, INFO, WARN, ERR, NO_PRINT)
 */
void driver::setPrintLvl(LogLevel lvl) { setDbgLvlExtended(static_cast<int>(lvl)); }

LogLevel driver::getPrintLvl() { return static_cast<LogLevel>(getDbgLvlExtended()); }

void driver::forceDebugSync(LogLevel lvl) { setDbgLvlExtended(static_cast<int>(lvl)); }

/**
 * @brief Initializes the Level Zero API.
 *
 * This function initializes the Level Zero API with the ZE_INIT_FLAG_GPU_ONLY flag.
 * It retrieves the number of available drivers and their handles.
 *
 * @return ze_result_t indicating success or failure.
 */
ze_result_t driver::zeInitialize()
{
	TRACING();
	ze_result_t result = zeInit(ZE_INIT_FLAG_GPU_ONLY);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to initialize Level Zero. Error code: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("ZE API initialized successfully.\n");

	// Discover the number of ze driver instances
	result = zeDriverGet(&driverCount, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get driver handles: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	// Allocate space for ze driver handles
	zeDrivers = new ze_driver_handle_t[driverCount];
	if (zeDrivers == nullptr) {
		ERR("Failed to allocate memory for driver handles.\n");
		return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
	}
	memset(zeDrivers, 0, sizeof(ze_driver_handle_t) * driverCount);

	DBG("Number of zeDrivers: {}\n", driverCount);

	// Retrieve driver handles
	result = zeDriverGet(&driverCount, zeDrivers);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get driver handles: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Initializes the ZES (Ze Management Services) API.
 *
 * This function initializes the ZES API with the ZE_INIT_FLAG_GPU_ONLY flag.
 * It retrieves the number of available ZES drivers and their handles, as well as all ZES devices.
 *
 * @return ze_result_t indicating success or failure.
 */
ze_result_t driver::zesInitialize()
{
	TRACING();
	uint32_t zesDriverCount = 0;

	ze_result_t result = zesInit(ZE_INIT_FLAG_GPU_ONLY);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to initialize ZES API: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("ZES API initialized successfully.\n");

	// Discover the number of zes driver instances
	result = zesDriverGet(&zesDriverCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || zesDriverCount == 0) {
		ERR("Failed to get driver count: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Number of zesDrivers: {}\n", zesDriverCount);

	// Allocate space for zes driver handles
	zesDrivers = new zes_driver_handle_t[zesDriverCount];
	if (zesDrivers == nullptr) {
		ERR("Failed to allocate memory for driver handles.\n");
		return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
	}
	memset(zesDrivers, 0, sizeof(zes_driver_handle_t) * zesDriverCount);

	// Retrieve driver handles
	result = zesDriverGet(&zesDriverCount, zesDrivers);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get driver handles: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	// The way that zes devices work is that they are associated with a single zes driver only.
	// So we have to enumerate all the zesDevices here, and then pass them to the device class.
	// There we can associate the zesDevices with the zeDevices.
	result = zesDeviceGet(zesDrivers[0], &totalZesDevicesCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || totalZesDevicesCount == 0) {
		ERR("Failed to get device count: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	totalZesDevices = new zes_device_handle_t[totalZesDevicesCount];
	if (totalZesDevices == nullptr) {
		ERR("Failed to allocate memory for zes device handles.\n");
		return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
	}
	memset(totalZesDevices, 0, sizeof(zes_device_handle_t) * totalZesDevicesCount);

	result = zesDeviceGet(zesDrivers[0], &totalZesDevicesCount, totalZesDevices);

	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device handles: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Initializes the driver and its associated devices.
 *
 * This function initializes the Level Zero and ZES APIs, retrieves driver handles,
 * and initializes each device associated with the drivers. It also sets the ZET_ENABLE_METRICS
 * environment variable.
 *
 * @return ze_result_t indicating success or failure.
 */
ze_result_t driver::init()
{
	TRACING();

	// Set ZET_ENABLE_METRICS environment variable
	SETENV("ZET_ENABLE_METRICS", "1");

	// Set COMPOSITE device hierarchy mode to ensure zeDeviceGet returns root devices.
	// zesDeviceGet always returns root device handles.
	// Setting to COMPOSITE mode ensures correct device mapping via UUID.
	SETENV("ZE_FLAT_DEVICE_HIERARCHY", "COMPOSITE");

	// Set ZE_ENABLE_PCI_ID_DEVICE_ORDER to ensure consistent device ordering
	SETENV("ZE_ENABLE_PCI_ID_DEVICE_ORDER", "1");

	ze_result_t result;
	// Temporary container to store zes devices which are not in survivability mode.
	std::set<std::string> devBdfs{};

	result = zesInitialize();
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	// In survivability mode, zeInit might fail. However, we should not exit early
	// because zesInit may still succeed, and the handle is needed for survivability features like
	// upgrading the firmware of the device.
	result = zeInitialize();
	if (result == ZE_RESULT_SUCCESS) {
		// Use local vector to avoid memset on non-trivial type
		std::vector<devGroup> localDevs(driverCount);

		for (uint32_t i = 0; i < driverCount; i++) {
			ze_api_version_t apiVersion = {};
			result = zeDriverGetApiVersion(zeDrivers[i], &apiVersion);
			if (result == ZE_RESULT_SUCCESS) {
#define PRINT_API_VERSION(i, x)                                                                                        \
	case x:                                                                                                            \
		DBG("Driver {} API Version: {}\n", i, #x);                                                                     \
		break;

				switch (apiVersion) {
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
			} else {
				ERR("Failed to get API version for driver {}. Error code: {}\n", i, result);
				return result;
			}

			DBG("\n==============================================\n");

			// Get zeDevices associated with the driver
			result = zeDeviceGet(zeDrivers[i], &localDevs[i].totalDevicesCount, nullptr);
			if (result != ZE_RESULT_SUCCESS) {
				ERR("Failed to get driver handles: 0x{:X} ({})\n", result, l0_error_to_string(result));
				return result;
			}

			// Resize vectors for zeDevices and dev
			localDevs[i].zeDevices.resize(localDevs[i].totalDevicesCount);
			localDevs[i].dev.resize(localDevs[i].totalDevicesCount);

			// Retrieve zeDevices for the driver
			result = zeDeviceGet(zeDrivers[i], &localDevs[i].totalDevicesCount, localDevs[i].zeDevices.data());
			if (result != ZE_RESULT_SUCCESS) {
				ERR("Failed to get device handles: 0x{:X} ({})\n", result, l0_error_to_string(result));
				return result;
			}

			for (uint32_t j = 0; j < localDevs[i].totalDevicesCount; j++) {
				DBG("Driver {} Device {}: {}\n", i, j, (void *)localDevs[i].zeDevices[j]);

				// Initialize each device in the group
				result = localDevs[i].dev[j].init(zeDrivers[i], zesDrivers[0], localDevs[i].zeDevices[j],
												  totalZesDevices, +totalZesDevicesCount);
				devBdfs.insert(localDevs[i].dev[j].getBDFStr());
				if (result != ZE_RESULT_SUCCESS) {
					ERR("Failed to initialize device for driver {}. Error code: {}\n", i, result);
					return result;
				}
			}
		}

		// Copy from local vector to raw array for DLL boundary
		devs = new devGroup[driverCount];
		for (uint32_t i = 0; i < driverCount; i++) {
			devs[i] = std::move(localDevs[i]);
		}
	}

	// Find zes devices which are in survivability mode.
	if (devBdfs.size() < totalZesDevicesCount) {
		svZesDevs.zesDriver = zesDrivers[0];
		uint32_t survDevCount = static_cast<uint32_t>(totalZesDevicesCount - devBdfs.size());
		svZesDevs.survDevices = new device[survDevCount];
		svZesDevs.survDevCount = survDevCount;
		uint32_t devIndex = 0;
		for (uint32_t k = 0; k < totalZesDevicesCount; k++) {
			zes_pci_properties_t pciProps{};
			result = zesDevicePciGetProperties(totalZesDevices[k], &pciProps);
			char bdfStr[BDF_STR_LEN];
			snprintf(bdfStr, sizeof(bdfStr), "%04x:%02x:%02x.%01x", pciProps.address.domain, pciProps.address.bus,
					 pciProps.address.device, pciProps.address.function);
			if (!devBdfs.contains(bdfStr) && (devIndex < survDevCount)) {
				svZesDevs.survDevices[devIndex].smDevInit(zesDrivers[0], totalZesDevices[k]);
				svZesDevs.survDevices[devIndex].setSurvivabilityMode(true);
				devIndex++;
			}
		}
	}

	initialized = true;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Destructor for the driver class.
 *
 * This destructor releases the memory allocated for driver and device handles.
 */
driver::~driver()
{
	if (devs != nullptr) {
		delete[] devs;
		devs = nullptr;
	}
	// Clean up the ZE driver handles
	if (zeDrivers != nullptr) {
		delete[] zeDrivers;
		zeDrivers = nullptr;
	}
	// Clean up the ZES driver handles
	if (zesDrivers != nullptr) {
		delete[] zesDrivers;
		zesDrivers = nullptr;
	}

	// Clean up the ZES device handles
	if (totalZesDevices != nullptr) {
		delete[] totalZesDevices;
		totalZesDevices = nullptr;
	}

	// Clean up survivability devices
	if (svZesDevs.survDevices != nullptr) {
		delete[] svZesDevs.survDevices;
		svZesDevs.survDevices = nullptr;
		svZesDevs.survDevCount = 0;
	}
}

/**
 * @brief Retrieves and prints the extension properties for a given Level Zero driver.
 *
 * This function retrieves and prints the extension properties supported by the specified Level Zero driver.
 *
 * @param driver The handle to the Level Zero driver.
 * @return ze_result_t indicating success or failure.
 */
ze_result_t driver::getExtensionProperties(ze_driver_handle_t drvr)
{
	uint32_t extensionCount = 0;
	ze_result_t result = zeDriverGetExtensionProperties(drvr, &extensionCount, NULL);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get extension count. Error code: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	if (extensionCount == 0) {
		DBG("No extensions available for this driver.\n");
		return ZE_RESULT_SUCCESS;
	}

	std::vector<ze_driver_extension_properties_t> extensions(extensionCount);

	result = zeDriverGetExtensionProperties(drvr, &extensionCount, extensions.data());
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get extension properties. Error code: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Extension Properties:\n");
	for (uint32_t i = 0; i < extensionCount; ++i) {
		DBG("  Extension {}: {}, version 0x{:X}\n", i + 1, extensions[i].name, extensions[i].version);
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Retrieves and prints the properties of a given Level Zero driver.
 *
 * This function retrieves and prints the properties of the specified Level Zero driver,
 * including its UUID and driver version.
 *
 * @param driver The handle to the Level Zero driver.
 * @return ze_result_t indicating success or failure.
 */
ze_result_t driver::getDriverProperties(ze_driver_handle_t drvr)
{
	ze_driver_properties_t driverProperties = {};
	ze_result_t result = zeDriverGetProperties(drvr, &driverProperties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get driver properties. Error code: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Driver Properties:\n");
	DBG("  UUID: ");
	for (int j = 0; j < ZE_MAX_DRIVER_UUID_SIZE; ++j) {
		DBG("{:02x}", driverProperties.uuid.id[j]);
	}
	DBG("\n");
	DBG("  Driver Version: 0x{:X}\n", driverProperties.driverVersion);
	return result;
}

/**
 * @brief Retrieves and prints the IPC (Inter-Process Communication) properties of a Level Zero driver.
 *
 * This function retrieves and prints the IPC properties of the specified Level Zero driver,
 * indicating whether it supports passing memory allocations and event pools between processes.
 *
 * @param driver The handle to the Level Zero driver.
 * @return ze_result_t indicating success or failure.
 */
ze_result_t driver::getIpcProperties(ze_driver_handle_t drvr)
{
	ze_driver_ipc_properties_t ipcProperties;
	ze_result_t result = zeDriverGetIpcProperties(drvr, &ipcProperties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get IPC properties. Error code: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	// If successful, print the IPC properties
	DBG("IPC Properties:\n");
	DBG("  Supports passing memory allocations between processes: {}\n",
		ipcProperties.flags & ZE_IPC_PROPERTY_FLAG_MEMORY ? "Yes" : "No");
	DBG("  Supports passing event pools between processes: {}\n",
		ipcProperties.flags & ZE_IPC_PROPERTY_FLAG_EVENT_POOL ? "Yes" : "No");

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Runs the functions related to the driver class and its associated devices.
 *
 * This function iterates over each driver and its associated devices, retrieving driver properties,
 * IPC properties, and extension properties. It then runs the device operations for each device.
 *
 * @return ze_result_t indicating success or failure.
 */
ze_result_t driver::run()
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	// Check if the driver has been initialized
	if (!initialized) {
		ERR("Driver not initialized. Call init() first.\n");
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	// Iterate over each driver and get extension properties
	for (uint32_t i = 0; i < driverCount; i++) {
		getDriverProperties(zeDrivers[i]);
		getIpcProperties(zeDrivers[i]);
		getExtensionProperties(zeDrivers[i]);

		// Run device operations
		for (uint32_t j = 0; j < devs[i].totalDevicesCount; j++) {
			result = devs[i].dev[j].run();
			if (result != ZE_RESULT_SUCCESS) {
				ERR("Failed to run device operations: 0x{:X} ({})\n", result, l0_error_to_string(result));
			}
		}
	}
	return result;
}

/**
 * @brief Gets the versions of the Level Zero loader components.
 *
 * This function retrieves and prints the versions of the Level Zero loader components.
 */
void driver::getLoaderVersion(std::string *lzVersion)
{
	zel_component_version_t *versions;
	size_t size = 0;
	zelLoaderGetVersions(&size, nullptr);
	DBG("zelLoaderGetVersions number of components found: {}\n", size);
	versions = new zel_component_version_t[size];
	zelLoaderGetVersions(&size, versions);

	if (size > 0) {
		*lzVersion = std::to_string(versions[0].component_lib_version.major) + ".";
		*lzVersion += std::to_string(versions[0].component_lib_version.minor) + ".";
		*lzVersion += std::to_string(versions[0].component_lib_version.patch);
	}

	delete[] versions;
}

/*
 * @brief Retrieves logs and saves them to a specified file.
 *
 * This function retrieves logs using the GETLOGS macro and saves them to the specified file.
 *
 * @param fileName The name of the file to save the logs to.
 * @return ze_result_t indicating success or failure.
 */
ze_result_t driver::getLogs(UNUSED std::string fileName)
{
	TRACING();
	// Result variable prevents MSVC /analyze warning C6326: Potential comparison of a constant with another constant
	int result = GETLOGS(fileName);
	return (result == 0) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

/**
 * @brief Resolves a single non-comma token (BDF address, numeric index, or empty) to a device.
 *
 * Empty @p token adds all devices. A purely numeric token matches by device index.
 * Any other token is matched against each device's BDF address.
 *
 * @param[in]  token   Single specifier: empty = all devices, numeric index, or BDF address.
 *                     Must not contain commas — use @c findDevice for comma-separated input.
 * @param[out] devList Vector to append matching devices to.
 * @return @c ZE_RESULT_SUCCESS on success, @c ZE_RESULT_ERROR_INVALID_ARGUMENT if @p token
 *         is non-empty and does not match any known device.
 */
ze_result_t driver::findOneToken(std::string_view token, std::vector<devInfo> *devList)
{
	// Null-terminated copy required by isBDF (const char* API) — allocated once, outside the loop
	const std::string bdfStr{token};

	const std::optional<uint32_t> numericId = xpum::detail::parseUint32(token);

	const auto sizeBefore = devList->size();

	// Empty token means "all devices" — caller passed nullptr or "".
	if (token.empty()) {
		uint32_t deviceIndex = 0;
		for (devGroup &dg : std::span{devs, driverCount}) {
			for (device &dev : dg.dev) {
				DBG("No BDF provided, adding all devices.\n");
				dev.addInfo(devList, deviceIndex++);
			}
		}
	} else {
		uint32_t deviceIndex = 0;
		for (devGroup &dg : std::span{devs, driverCount}) {
			for (device &dev : dg.dev) {
				if (dev.isBDF(bdfStr.c_str())) {
					dev.addInfo(devList, deviceIndex);
					return ZE_RESULT_SUCCESS;
				}
				if (numericId && *numericId == deviceIndex) {
					DBG("Found device with index: {}\n", *numericId);
					dev.addInfo(devList, deviceIndex);
					return ZE_RESULT_SUCCESS;
				}
				deviceIndex++;
			}
		}
	}

	if (!token.empty() && devList->size() == sizeBefore) {
		ERR("Device not found: '{}'\n", bdfStr);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Finds one or more devices by BDF address, numeric index, or comma-separated list.
 *
 * Accepts a single token or a comma-separated list, e.g. @c "0,1" or @c "0000:03:00.0,1".
 * Each token is resolved via @c findOneToken independently, results are deduplicated by
 * device index while preserving order. Unrecognised tokens emit an error and are skipped;
 * the call succeeds as long as at least one token matched. A single unrecognised token
 * (no comma) returns @c ZE_RESULT_ERROR_INVALID_ARGUMENT. If @p bdf is nullptr or empty,
 * all devices are added.
 *
 * @param[in]  bdf     Device specifier: nullptr/empty = all, numeric index, BDF address,
 *                     or comma-separated combination of the above.
 * @param[out] devList Vector to append matching devices to.
 * @return @c ZE_RESULT_SUCCESS if at least one token matched (or input was empty).
 *         @c ZE_RESULT_ERROR_INVALID_ARGUMENT if a single token did not match any device.
 */
ze_result_t driver::findDevice(const char *bdf, std::vector<devInfo> *devList)
{
	const std::string_view bdfView{bdf != nullptr ? bdf : ""};

	if (bdfView.find(',') == std::string_view::npos) {
		return findOneToken(bdfView, devList);
	}

	const auto sizeBefore = devList->size();
	for (const auto token : bdfView | std::views::split(',')) {
		const std::string_view sv{token.begin(), token.end()};
		if (sv.empty()) {
			continue;
		}
		// Errors (device not found) are already logged by findOneToken; skip and continue.
		findOneToken(sv, devList);
	}
	xpum::detail::deduplicateByIndex(*devList);
	if (devList->size() == sizeBefore) {
		return ZE_RESULT_ERROR_INVALID_ARGUMENT; // no token matched anything
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Finds a survivability device based on its BDF address or index.
 *
 * This function searches for a survivability device based on its BDF address. If no BDF is
 * provided, it adds all survivability devices to the list.
 *
 * @param [in] bdf The BDF address of the device to find. If nullptr or empty, all survivability devices are added.
 * @param [out] survDevList A pointer to a vector to store the survivability device information.
 */
void driver::findSurvDevice(const char *bdf, std::vector<devInfo> *survDevList)
{
	// Surv device indices continue sequentially after all normal devices
	uint32_t deviceIndex = 0;
	for (uint32_t i = 0; i < driverCount; i++) {
		deviceIndex += devs[i].totalDevicesCount;
	}

	const std::string_view bdfView{bdf ? bdf : ""};
	const std::optional<uint32_t> numericId = xpum::detail::parseUint32(bdfView);

	for (uint32_t k = 0; k < svZesDevs.survDevCount; k++) {
		device &dev = svZesDevs.survDevices[k];
		if (bdfView.empty()) {
			dev.addInfo(survDevList, deviceIndex);
		} else {
			if (dev.isBDF(bdf)) {
				dev.addInfo(survDevList, deviceIndex);
				return;
			} else if (numericId && *numericId == deviceIndex) {
				dev.addInfo(survDevList, deviceIndex);
				return;
			}
		}
		deviceIndex++;
	}
}
