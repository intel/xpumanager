/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _DRIVER_H
#define _DRIVER_H

#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include "cperlog.h"
#include "device.h"

struct survivabilityDevices
{
	zes_driver_handle_t zesDriver;
	uint32_t survDevCount;
	device *survDevices;
};

/**
 * @brief One component reported by the Level Zero loader.
 *
 * zelLoaderGetVersions() lists the loader itself and whichever layers are enabled
 * (tracing, validation) -- never the drivers, so no component identifies the
 * user-mode driver version.
 */
struct LoaderComponent
{
	std::string name;		 ///< Component name as the loader reports it, e.g. "loader".
	std::string version;	 ///< Component library version, "major.minor.patch".
	std::string specVersion; ///< Level Zero API level the component implements, "major.minor".
};

struct devGroup
{
	uint32_t totalDevicesCount;
	std::vector<device> dev;
	std::vector<ze_device_handle_t> zeDevices;
};

class LIBXPUM_API driver
{
private:
	ze_result_t findOneToken(std::string_view token, std::vector<devInfo> *devList);
	// Builds the device list from sysman handles only (no zeInit). Used by the
	// config --reset path; see init(bool skipZeInit).
	ze_result_t smOnlyEnumerate();
	bool initialized;
	uint32_t driverCount;
	uint32_t totalZesDevicesCount;
	ze_driver_handle_t *zeDrivers;
	zes_driver_handle_t *zesDrivers;
	zes_device_handle_t *totalZesDevices;
	devGroup *devs;
	survivabilityDevices svZesDevs;
	// Devices Level Zero does not report, created on demand by findDeviceForRecovery().
	std::vector<std::unique_ptr<device>> pciOnlyDevs;
	[[nodiscard]] device *pciOnlyDevice(const std::string &bdf);
	// The reported device groups; empty until init() has built them, even when it failed partway.
	[[nodiscard]] std::span<devGroup> reportedGroups() const
	{
		return devs == nullptr ? std::span<devGroup>{} : std::span<devGroup>{devs, driverCount};
	}

public:
	driver()
		: initialized(false), driverCount(0), totalZesDevicesCount(0), zeDrivers(nullptr), zesDrivers(nullptr),
		  totalZesDevices(nullptr), devs(nullptr), svZesDevs{}
	{}
	// Owns raw driver and device arrays, and the DLL export would otherwise instantiate copies
	// of pciOnlyDevs, which holds unique_ptr.
	driver(const driver &) = delete;
	driver &operator=(const driver &) = delete;
	driver(driver &&) = delete;
	driver &operator=(driver &&) = delete;
	~driver();
	void setPrintLvl(LogLevel lvl);
	LogLevel getPrintLvl();
	void forceDebugSync(LogLevel lvl);
	// When skipZeInit is true the Level Zero compute runtime (zeInit) is not
	// initialized; only sysman (zesInit) is brought up and devices are
	// enumerated via sysman. This is used on the config --reset path so the
	// compute runtime never opens render fds / GuC exec queues on a device we
	// are about to reset (which otherwise triggers an xe "Missing outer runtime
	// PM protection" kernel WARN when the stale queues are torn down at exit).
	ze_result_t init(bool skipZeInit = false);
	bool isDriverLoaded() { return initialized; }
	ze_result_t zeInitialize();
	ze_result_t zesInitialize();
	ze_result_t getDriverProperties(ze_driver_handle_t drvr);
	ze_result_t getIpcProperties(ze_driver_handle_t drvr);
	ze_result_t getExtensionProperties(ze_driver_handle_t drvr);
	[[nodiscard]] std::optional<std::string> getLoaderVersion();
	[[nodiscard]] std::optional<LoaderComponent> getLoaderComponent();
	ze_result_t findDevice(const char *bdf, std::vector<devInfo> *dev);
	void findSurvDevice(const char *bdf, std::vector<devInfo> *survDev);
	ze_result_t findDeviceForRecovery(const char *bdf, std::vector<devInfo> *devList);
	ze_result_t getLogs(std::string fileName);

	// Crash log management, delegated to the OS layer (Intel Crash Log CLI on
	// Linux; unsupported on Windows).
	bool crashlogSupported();
	ze_result_t crashlogListSources(std::set<std::string> &bdfs, std::string &output);
	ze_result_t crashlogControl(const std::string &verb, const std::string &bdf, std::string &output);
	ze_result_t crashlogExtract(const std::string &bdf, const std::string &outputDir, std::vector<std::string> &files,
								std::string &output);
	ze_result_t crashlogDecode(const std::string &inputFile, const std::string &jsonFile, std::string &output);

	[[nodiscard("ZE_RESULT_WARNING_DROPPED_DATA signals partial data; callers must not treat it as success")]]
	ze_result_t getCperLog(std::vector<uint8_t> &cperBlob, std::optional<std::string_view> instanceName = std::nullopt,
						   CperBufferSizeKb bufferSizeKb = std::nullopt, bool peek = false);
	[[nodiscard("ZE_RESULT_WARNING_DROPPED_DATA signals partial data; callers must not treat it as success")]]
	ze_result_t getCperLogWithMetadata(std::vector<uint8_t> &cperBlob,
									   std::vector<zes_intel_info_log_metadata_exp> &metadata,
									   std::optional<std::string_view> instanceName = std::nullopt,
									   CperBufferSizeKb bufferSizeKb = std::nullopt, bool peek = false);
	ze_result_t run();
};

#endif
