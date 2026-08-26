/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ENGINEGROUP_H
#define _ENGINEGROUP_H

#include "sysman.h"
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

/**
 * @brief One engine group busyness counter reading, tagged with its group type and tile.
 *
 * Produced by @c enginegroup::getAllEngineActivity, consumed by
 * @c enginegroup::computeGpuUtilPerTile. @c activeTime and @c timestamp share the
 * driver's clock, so only their ratio is meaningful — the unit is not specified by
 * the Level Zero sysman API and is not wall-clock microseconds on every driver.
 */
struct EngineActivitySample
{
	/**
	 * Engine group type reported by sysman. Defaults to the enum's sentinel rather than to a
	 * real group, so a sample that was never filled in cannot be mistaken for a reading of
	 * ZES_ENGINE_GROUP_ALL — the group whose device-wide average this type exists to avoid.
	 */
	zes_engine_group_t type = ZES_ENGINE_GROUP_FORCE_UINT32;
	uint32_t tileId = 0;	 /**< subdevice (tile) id; 0 for single-tile devices */
	uint64_t activeTime = 0; /**< cumulative busy time in driver clock units */
	uint64_t timestamp = 0;	 /**< snapshot time in driver clock units */
};

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
	[[nodiscard]] ze_result_t getAllEngineActivity(std::vector<EngineActivitySample> &samples);

	[[nodiscard]] static std::map<uint32_t, double>
	computeGpuUtilPerTile(const std::vector<EngineActivitySample> &before,
						  const std::vector<EngineActivitySample> &after);
	[[nodiscard]] static std::map<uint32_t, double>
	computeGroupUtilPerTile(const std::vector<EngineActivitySample> &before,
							const std::vector<EngineActivitySample> &after, zes_engine_group_t group);
	[[nodiscard]] static std::optional<double> deviceUtilFromTiles(const std::map<uint32_t, double> &utilPerTile);

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif
