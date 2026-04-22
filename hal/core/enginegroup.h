/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ENGINEGROUP_H
#define _ENGINEGROUP_H

#include "sysman.h"
#include <map>
#include <span>
#include <tuple>
#include <utility>

class LIBXPUM_API enginegroup : public sysman
{
private:
	uint32_t engineGroupCount;
	zes_engine_handle_t *engineGroups;

public:
	enginegroup() : engineGroupCount(0), engineGroups(nullptr) {}
	~enginegroup();
	ze_result_t enumGroups(zes_device_handle_t device);
	ze_result_t getProperties(zes_engine_handle_t engineGroup, zes_engine_properties_t *engineProperties);
	ze_result_t getActivity(zes_engine_handle_t engineGroup, zes_engine_stats_t *engineStats);
	ze_result_t getActivityExt(zes_engine_handle_t engineGroup);
	ze_result_t getEngineCountByType(uint32_t *count, zes_engine_group_t type);
	std::tuple<ze_result_t, uint64_t, uint64_t> getUtilization(std::span<const zes_engine_group_t> typeTable);
	ze_result_t getEngineActivityByType(zes_engine_group_t type, uint32_t engineIndex, uint64_t *activeTime,
										uint64_t *timestamp);
	ze_result_t getEngineActivityPerTile(zes_engine_group_t type,
										 std::map<uint32_t, std::pair<uint64_t, uint64_t>> &tileActivity);

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif
