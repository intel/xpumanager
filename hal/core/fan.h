/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _FAN_H
#define _FAN_H

#include "sysman.h"
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
	ze_result_t setFixedSpeedPercent(int32_t speedPercent, int32_t fanId = -1);
	ze_result_t setDefaultMode(int32_t fanId = -1);
	ze_result_t setSpeedTableMode(const std::vector<std::pair<uint32_t, int32_t>> &table, zes_fan_speed_units_t units,
								  int32_t fanId = -1);
};

#endif
