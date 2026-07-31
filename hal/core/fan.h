/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _FAN_H
#define _FAN_H

#include "sysman.h"
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

class LIBXPUM_API fan : public sysman
{
private:
	zes_device_handle_t deviceHandle;
	uint32_t fanCount;
	zes_fan_handle_t *fanHandles;
	zes_fan_properties_t *fanPropsCache;
	bool *fanPropsCached;
	zes_fan_config_t *fanConfigCache;
	bool *fanConfigCached;
	uint64_t *fanConfigCachedAtMs;
	void clearFanHandles();

	// sysfs hwmon fan RPM fallback (Battlemage/xe, validated on the Arc Pro B70).
	//
	// On the tested B70 the Level Zero sysman layer enumerates ZERO fan handles,
	// so zesFanGetState() cannot report a fan speed. The kernel xe driver does
	// expose the RPM through the PCI device's hwmon nodes (fanN_input). The
	// OS-specific sysfs traversal lives in the OS Abstraction Layer
	// (getHwmonInputPaths, oal/lin/hwmon_fan.cpp); resolveSysfsHwmon calls it once
	// and caches the resulting {zero-based fan index -> absolute fanN_input path}
	// map. The single-fan getter reads fan index 0's path; getAllSpeedsRpm reads
	// every cached path, reporting each by its real fan id. The portable
	// parse/read/selection helpers live in hwmon_fan_utils.h (namespace
	// xpum::hwmon); the getters below call into them with an oal-resolved path.
	std::map<uint32_t, std::string> sysfsFanInputPaths;
	void resolveSysfsHwmon(zes_device_handle_t device);
	ze_result_t readSysfsFanRpm(uint32_t fanIndex, int32_t *rpm);
	ze_result_t readAllSysfsFanRpms(std::map<uint32_t, int32_t> &rpms);

	ze_result_t ensureFansEnumerated();
	ze_result_t resolveTargetFanIndexes(int32_t fanId, std::vector<uint32_t> &targetIndexes);
	ze_result_t getPropertiesById(uint32_t fanId, zes_fan_properties_t &props);
	ze_result_t getConfigById(uint32_t fanId, zes_fan_config_t &config);
	void invalidateFanConfigCacheById(uint32_t fanId);
	uint64_t getMonotonicMs() const;
	void printSupportedModes(const uint32_t mode);
	ze_result_t enumFans(zes_device_handle_t device);
	ze_result_t getProperties(zes_fan_handle_t fanHandle, zes_fan_properties_t &properties);

public:
	fan()
		: deviceHandle(nullptr), fanCount(0), fanHandles(nullptr), fanPropsCache(nullptr), fanPropsCached(nullptr),
		  fanConfigCache(nullptr), fanConfigCached(nullptr), fanConfigCachedAtMs(nullptr)
	{}
	~fan();
	ze_result_t getConfig(zes_fan_config_t &config, int32_t fanId = 0);
	ze_result_t getFanCount(uint32_t &count);

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device) override;
	ze_result_t getSpeedPercentById(uint32_t fanId, int32_t &pct);
	ze_result_t getSpeedPercent(int32_t &pct);

	// Read the RPM of a specific Level Zero fan handle. Propagates the driver's
	// state-read error (never masked with sysfs) and accepts rpm >= 0 (0 == a
	// stopped fan, a valid reading).
	ze_result_t getSpeedRpmById(uint32_t fanId, int32_t &rpm);

	// Read every fan's RPM keyed by its real fan id. Routes through
	// xpum::hwmon::decideFanRpmSource: propagate an enumeration failure; on a
	// Level Zero device iterate all handles (skipping an individually
	// unavailable fan, propagating a hard driver error); on the Battlemage/xe
	// zero-handle case enumerate all fanN_input hwmon nodes.
	ze_result_t getAllSpeedsRpm(std::map<uint32_t, int32_t> &rpms);

	// Single-fan RPM convenience reader (fan index 0), kept for API stability.
	ze_result_t getSpeedRpm(int32_t &rpm);

	ze_result_t setFixedSpeedPercent(int32_t speedPercent, int32_t fanId = -1);
	ze_result_t setDefaultMode(int32_t fanId = -1);
	ze_result_t setSpeedTableMode(const std::vector<std::pair<uint32_t, int32_t>> &table, zes_fan_speed_units_t units,
								  int32_t fanId = -1);
};

#endif
