/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _POWER_H
#define _POWER_H

#include "sysman.h"
#include <vector>
#include <cstdint>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <fstream>
#include <functional>

#define POWER_MONITOR_INTERNAL_PERIOD 80
#define DEFAULT_THROTTLE_POWER 300
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

// Device-specific power thresholds to avoid DLL interface issues with STL containers
class PowerThresholds
{
private:
	std::map<uint32_t, uint64_t> healthDeviceToTdps;

public:
	PowerThresholds() = default;
	~PowerThresholds() = default;

	// Getter - return default if not found
	uint64_t getThrottlePower(uint32_t deviceId, uint64_t defaultValue) const;

	// Setter for loading configuration
	void setThrottlePower(uint32_t deviceId, uint64_t value);
};

class LIBXPUM_API power : public sysman
{
private:
	uint32_t powerCount;
	zes_pwr_handle_t *powerHandles;
	zes_device_handle_t zeDeviceHandle;
	device *deviceHandle;
	PowerThresholds *thresholds;
	uint64_t defaultThrottlePower;

public:
	power();
	~power();
	ze_result_t enumPowerDomains(zes_device_handle_t device);
	ze_result_t getProperties(zes_pwr_handle_t powerHandle, zes_power_properties_t *properties,
							  zes_power_ext_properties_t *extProps);
	ze_result_t getEnergyCounter(zes_pwr_handle_t powerHandle, zes_power_energy_counter_t *energyCounter);
	ze_result_t getEnergyThreshold(zes_pwr_handle_t powerHandle);
	ze_result_t getPowerLimits(zes_pwr_handle_t powerHandle);
	ze_result_t getEnergy(uint64_t *power, uint64_t *timeStamp, bool forGPU);
	ze_result_t getEnergyPerTile(std::map<uint32_t, std::pair<uint64_t, uint64_t>> &tileEnergy);
	ze_result_t setSustainedLimit(uint32_t limit_mw, int32_t tile_id = -1);
	ze_result_t setBurstLimit(uint32_t limit_mw);
	ze_result_t setPeakLimit(uint32_t limit_ac_mw, uint32_t limit_dc_mw);
	ze_result_t getLimitsExt(std::vector<PowerLimitExt> &limits);
	ze_result_t setLimitsExt(int32_t tile_id, zes_power_level_t level, uint32_t limit_mw);
	ze_result_t getDomainLimits(std::map<zes_power_domain_t, std::map<zes_power_level_t, int32_t>> &domainLimits);
	ze_result_t getMaxPowerLimit(std::string &range);

	uint32_t getPowerCount() { return powerCount; }
	uint64_t getThrottlePower(uint32_t pciDeviceId);
	zes_pwr_handle_t *getPowerHandles() { return powerHandles; }
	void loadPowerThresholds();
	void loadThresholdSection(const nlohmann::json &thresholdsJson, const std::string &key,
							  std::function<void(uint32_t, uint64_t)> setter);
	ze_result_t setPowerLimit(double powerLimit);

	// Helper functions for JSON threshold loading
	static uint32_t parseDeviceId(const std::string &hexKey);
	ze_result_t init(zes_device_handle_t zeDevice) override;
	ze_result_t init(zes_device_handle_t zeDevice, class device *device);
	ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif