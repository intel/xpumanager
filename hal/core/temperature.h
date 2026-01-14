/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _TEMPERATURE_H
#define _TEMPERATURE_H

#include "sysman.h"
#include <map>

#define CORE_THROTTLE_THRESHOLD_DEFAULT 105
#define CORE_SHUTDOWN_THRESHOLD_DEFAULT 130
#define MEMORY_THROTTLE_THRESHOLD_DEFAULT 85
#define MEMORY_SHUTDOWN_THRESHOLD_DEFAULT 100

// Maximum reasonable temperature threshold for filtering out erroneous sensor readings
// Sensors returning values >= this are likely reporting errors or invalid data, set
// in legacy code as 150.0 Celsius.
#define MAX_REASONABLE_TEMP_CELSIUS 150.0

class LIBXPUM_API temperature : public sysman
{
private:
	uint32_t temperatureCount;
	zes_temp_handle_t *temperatureHandles;

public:
	temperature() : temperatureCount(0), temperatureHandles(nullptr) {}
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
};
#endif