/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _DRIVER_H
#define _DRIVER_H

#include <set>
#include <string_view>
#include <vector>
#include "device.h"

struct survivabilityDevices
{
	zes_driver_handle_t zesDriver;
	uint32_t survDevCount;
	device *survDevices;
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

public:
	driver()
		: initialized(false), driverCount(0), totalZesDevicesCount(0), zeDrivers(nullptr), zesDrivers(nullptr),
		  totalZesDevices(nullptr), devs(nullptr), svZesDevs{}
	{}
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
	void getLoaderVersion(std::string *lzVersion);
	ze_result_t findDevice(const char *bdf, std::vector<devInfo> *dev);
	void findSurvDevice(const char *bdf, std::vector<devInfo> *survDev);
	ze_result_t getLogs(std::string fileName);

	// Crash log management, delegated to the OS layer (Intel Crash Log CLI on
	// Linux; unsupported on Windows).
	bool crashlogSupported();
	ze_result_t crashlogListSources(std::set<std::string> &bdfs, std::string &output);
	ze_result_t crashlogControl(const std::string &verb, const std::string &bdf, std::string &output);
	ze_result_t crashlogExtract(const std::string &bdf, const std::string &outputDir, std::vector<std::string> &files,
								std::string &output);
	ze_result_t crashlogDecode(const std::string &inputFile, const std::string &jsonFile, std::string &output);

	ze_result_t run();
};

#endif
