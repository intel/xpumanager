/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _DRIVER_H
#define _DRIVER_H

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
	device *dev;
	ze_device_handle_t *zeDevices;
};

class LIBXPUM_API driver
{
private:
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
	void setPrintLvl(int lvl);
	int getPrintLvl();			  // Add getter for current debug level
	void forceDebugSync(int lvl); // Add forced synchronization method
	ze_result_t init();
	bool isDriverLoaded() { return initialized; }
	ze_result_t zeInitialize();
	ze_result_t zesInitialize();
	ze_result_t getDriverProperties(ze_driver_handle_t drvr);
	ze_result_t getIpcProperties(ze_driver_handle_t drvr);
	ze_result_t getExtensionProperties(ze_driver_handle_t drvr);
	void getLoaderVersion(std::string *lzVersion);
	ze_result_t findDevice(const char *bdf, std::vector<devInfo> *dev);
	ze_result_t getLogs(std::string fileName);
	ze_result_t run();
};

#endif
