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
#ifndef _POWER_H
#define _POWER_H

#include "sysman.h"
#include <vector>
#include <cstdint>

/**
 * @brief Extended power limit descriptor
 *
 * This structure represents an extended power limit configuration for a device
 * or subdevice power domain. It provides information about specific power level
 * settings, current limit values, and whether the limit is locked.
 */
struct PowerLimitExt
{
	zes_power_level_t level; ///< Power level type (e.g., sustained, burst, peak)
	uint32_t limitMw;		 ///< Power limit value
	bool locked;			 ///< Indicates if the power limit is locked and cannot be modified
};

class device; // Forward declaration
class LIBXPUM_API power : public sysman
{
private:
	uint32_t powerCount;
	zes_pwr_handle_t *powerHandles;
	zes_device_handle_t zeDeviceHandle;
	device *deviceHandle;

public:
	power() : powerCount(0), powerHandles(nullptr), zeDeviceHandle(nullptr), deviceHandle(nullptr) {}
	~power();
	ze_result_t enumPowerDomains(zes_device_handle_t device);
	ze_result_t getProperties(zes_pwr_handle_t powerHandle, zes_power_properties_t *properties,
							  zes_power_ext_properties_t *extProps);
	ze_result_t getEnergyCounter(zes_pwr_handle_t powerHandle, zes_power_energy_counter_t *energyCounter);
	ze_result_t getEnergyThreshold(zes_pwr_handle_t powerHandle);
	ze_result_t getPowerLimits(zes_pwr_handle_t powerHandle);
	ze_result_t getEnergy(uint64_t *power, uint64_t *timeStamp, bool forGPU);
	ze_result_t setSustainedLimit(uint32_t limit_mw, int32_t tile_id = -1);
	ze_result_t setBurstLimit(uint32_t limit_mw);
	ze_result_t setPeakLimit(uint32_t limit_ac_mw, uint32_t limit_dc_mw);
	ze_result_t getLimitsExt(std::vector<PowerLimitExt> &limits);
	ze_result_t setLimitsExt(int32_t tile_id, zes_power_level_t level, uint32_t limit_mw);

	ze_result_t setPowerLimit(double powerLimit);
	ze_result_t init(zes_device_handle_t zeDevice) override;
	ze_result_t init(zes_device_handle_t zeDevice, class device *device);
	ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif