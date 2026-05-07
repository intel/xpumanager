/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef POWEREXP_H
#define POWEREXP_H

#include <os.h>
#include "zes_api.h"
#include "debug.h"
#include <cstdint>
#include <vector>

struct power_usage_exp_t
{
	zes_power_domain_t domain; ///< Power domain type (card, package, stack, etc.)
	uint32_t instantPower;	   ///< Instantaneous power in milliwatts
	uint32_t averagePower;	   ///< Time-averaged power in milliwatts
};

struct power_limits_exp_t
{
	zes_power_domain_t domain; ///< Power domain type (card, package, stack, etc.)
	uint32_t limit;			   ///< Power limit in milliwatts
};

class LIBXPUM_API powerExp
{
private:
	zes_driver_handle_t zesDriver;
	zes_device_handle_t zesDevice;
	zes_pwr_handle_t *powerHandles = nullptr;
	uint32_t powerCount = 0;
	bool powerExpEnabled;

public:
	powerExp() : zesDriver(nullptr), zesDevice(nullptr), powerHandles(nullptr), powerCount(0), powerExpEnabled(false) {}
	~powerExp()
	{
		delete[] powerHandles;
		powerHandles = nullptr;
	}
	ze_result_t init(zes_driver_handle_t driver, zes_device_handle_t device);
	ze_result_t getPowerUsage(std::vector<power_usage_exp_t> &powerUsages);
	ze_result_t getPowerLimits(std::vector<power_limits_exp_t> &powerLimits);
	ze_result_t setPowerLimit(uint32_t limitMw);
	bool isPowerExpEnabled() const { return powerExpEnabled; }
};

#endif // POWEREXP_H