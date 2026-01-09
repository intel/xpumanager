/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _TEMPERATURE_H
#define _TEMPERATURE_H

#include "sysman.h"
#include <map>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <fstream>
#include <memory>
#include <functional>

#define CORE_THROTTLE_THRESHOLD_DEFAULT 105
#define CORE_SHUTDOWN_THRESHOLD_DEFAULT 130
#define MEMORY_THROTTLE_THRESHOLD_DEFAULT 85
#define MEMORY_SHUTDOWN_THRESHOLD_DEFAULT 100

// Maximum reasonable temperature threshold for filtering out erroneous sensor readings
// Sensors returning values >= this are likely reporting errors or invalid data, set
// in legacy code as 150.0 Celsius.
#define MAX_REASONABLE_TEMP_CELSIUS 150.0

// Device-specific temperature thresholds to avoid DLL interface issues with STL containers
class TemperatureThresholds
{
private:
	std::map<uint32_t, uint64_t> pHealthDeviceToThrottleCoreTemperatures;
	std::map<uint32_t, uint64_t> pHealthDeviceToShutdownCoreTemperatures;
	std::map<uint32_t, uint64_t> pHealthDeviceToShutdownMemoryTemperatures;

public:
	TemperatureThresholds() = default;
	~TemperatureThresholds() = default;

	// Getters - return default if not found
	uint64_t getThrottleCoreTemperature(uint32_t deviceId, uint64_t defaultValue) const;
	uint64_t getShutdownCoreTemperature(uint32_t deviceId, uint64_t defaultValue) const;
	uint64_t getShutdownMemoryTemperature(uint32_t deviceId, uint64_t defaultValue) const;

	// Setters for loading configuration
	void setThrottleCoreTemperature(uint32_t deviceId, uint64_t value);
	void setShutdownCoreTemperature(uint32_t deviceId, uint64_t value);
	void setShutdownMemoryTemperature(uint32_t deviceId, uint64_t value);
};

class LIBXPUM_API temperature : public sysman
{
private:
	uint32_t temperatureCount;
	zes_temp_handle_t *temperatureHandles;
	TemperatureThresholds *thresholds;

	// Default threshold values
	uint64_t defaultCoreThrottleThreshold;
	uint64_t defaultCoreShutdownThreshold;
	uint64_t defaultMemoryShutdownThreshold;

	void loadTemperatureThresholds();
	void loadThresholdSection(const nlohmann::json &thresholdsJson, const std::string &key,
							  std::function<void(uint32_t, uint64_t)> setter);

public:
	temperature();
	~temperature();
	ze_result_t enumTemperatureDomains(zes_device_handle_t device);
	ze_result_t getProperties(zes_temp_handle_t temperatureHandle, zes_temp_properties_t *properties);
	ze_result_t getState(zes_temp_handle_t temperatureHandle, double *temp);
	ze_result_t getTemp(zes_temp_sensors_t type, double *temp);
	ze_result_t getTempPerTile(zes_temp_sensors_t type, std::map<uint32_t, double> &tileTemperatures);
	ze_result_t getCoreTemp(double *coreTemp);
	ze_result_t getMemoryTemp(double *memTemp);
	ze_result_t getCoreThreshold(zes_device_handle_t device, uint32_t *throttleThreshold, uint32_t *shutdownThreshold);
	ze_result_t getMemoryThreshold(zes_device_handle_t device, uint32_t *throttleThreshold,
								   uint32_t *shutdownThreshold);

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device) override;
	uint64_t getThrottleCoreTemperature(uint32_t pciDeviceId);
	uint64_t getShutdownCoreTemperature(uint32_t pciDeviceId);
	uint64_t getShutdownMemoryTemperature(uint32_t pciDeviceId);

	// Helper functions for JSON threshold loading
	static uint32_t parseDeviceId(const std::string &hexKey);
};
#endif