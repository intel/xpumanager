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
#ifndef _TEMPERATURE_H
#define _TEMPERATURE_H

#include "sysman.h"

#define CORE_THROTTLE_THRESHOLD_DEFAULT 105
#define CORE_SHUTDOWN_THRESHOLD_DEFAULT 130
#define MEMORY_THROTTLE_THRESHOLD_DEFAULT 85
#define MEMORY_SHUTDOWN_THRESHOLD_DEFAULT 100

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
	ze_result_t getTemp(zes_device_handle_t device, zes_temp_sensors_t type, double *coreTemp);
	ze_result_t getCoreTemp(zes_device_handle_t device, double *coreTemp);
	ze_result_t getMemoryTemp(zes_device_handle_t device, double *coreTemp);
	ze_result_t getCoreThreshold(zes_device_handle_t device, uint32_t *throttleThreshold, uint32_t *shutdownThreshold);
	ze_result_t getMemoryThreshold(zes_device_handle_t device, uint32_t *throttleThreshold,
								   uint32_t *shutdownThreshold);

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device) override;
};
#endif