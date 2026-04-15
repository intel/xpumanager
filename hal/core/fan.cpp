/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "fan.h"
#include <algorithm>
#include <chrono>

namespace {
constexpr uint64_t FAN_CONFIG_CACHE_TTL_MS = 500;
}

/**
 * @brief Releases cached fan handles and resets fan count.
 */
void fan::clearFanHandles()
{
	if (fanHandles != nullptr) {
		delete[] fanHandles;
		fanHandles = nullptr;
	}
	if (fanPropsCache != nullptr) {
		delete[] fanPropsCache;
		fanPropsCache = nullptr;
	}
	if (fanPropsCached != nullptr) {
		delete[] fanPropsCached;
		fanPropsCached = nullptr;
	}
	if (fanConfigCache != nullptr) {
		delete[] fanConfigCache;
		fanConfigCache = nullptr;
	}
	if (fanConfigCached != nullptr) {
		delete[] fanConfigCached;
		fanConfigCached = nullptr;
	}
	if (fanConfigCachedAtMs != nullptr) {
		delete[] fanConfigCachedAtMs;
		fanConfigCachedAtMs = nullptr;
	}
	fanCount = 0;
}

uint64_t fan::getMonotonicMs() const
{
	using namespace std::chrono;
	return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void fan::invalidateFanConfigCacheById(uint32_t fanId)
{
	if (fanConfigCached == nullptr || fanConfigCachedAtMs == nullptr || fanId >= fanCount) {
		return;
	}
	fanConfigCached[fanId] = false;
	fanConfigCachedAtMs[fanId] = 0;
}

ze_result_t fan::getConfigById(uint32_t fanId, zes_fan_config_t &config)
{
	if (fanHandles == nullptr || fanConfigCache == nullptr || fanConfigCached == nullptr ||
		fanConfigCachedAtMs == nullptr || fanId >= fanCount) {
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	const uint64_t nowMs = getMonotonicMs();
	if (fanConfigCached[fanId]) {
		const uint64_t ageMs = nowMs - fanConfigCachedAtMs[fanId];
		if (ageMs <= FAN_CONFIG_CACHE_TTL_MS) {
			config = fanConfigCache[fanId];
			return ZE_RESULT_SUCCESS;
		}
	}

	zes_fan_config_t liveConfig = {};
	ze_result_t result = zesFanGetConfig(fanHandles[fanId], &liveConfig);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	fanConfigCache[fanId] = liveConfig;
	fanConfigCached[fanId] = true;
	fanConfigCachedAtMs[fanId] = nowMs;
	config = liveConfig;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Gets fan properties for a specific fan index, using cache to avoid repeated driver calls.
 *
 * Fan properties (maxRPM, canControl, supportedModes) are static and never change at runtime.
 * The result is stored in fanPropsCache on first fetch and returned directly on subsequent calls.
 *
 * @param[in]  fanId Fan index.
 * @param[out] props Populated fan properties.
 * @return ze_result_t ZE_RESULT_SUCCESS on success.
 */
ze_result_t fan::getPropertiesById(uint32_t fanId, zes_fan_properties_t &props)
{
	if (fanHandles == nullptr || fanPropsCache == nullptr || fanPropsCached == nullptr || fanId >= fanCount) {
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	if (fanPropsCached[fanId]) {
		props = fanPropsCache[fanId];
		return ZE_RESULT_SUCCESS;
	}
	ze_result_t result = getProperties(fanHandles[fanId], props);
	if (result == ZE_RESULT_SUCCESS) {
		fanPropsCache[fanId] = props;
		fanPropsCached[fanId] = true;
	}
	return result;
}

/**
 * @brief Lazily enumerates fans when the cache is empty.
 *
 * @return ze_result_t ZE_RESULT_SUCCESS when fan handles are ready.
 */
ze_result_t fan::ensureFansEnumerated()
{
	if (fanHandles != nullptr && fanCount > 0) {
		return ZE_RESULT_SUCCESS;
	}
	if (deviceHandle == nullptr) {
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}
	return enumFans(deviceHandle);
}

/**
 * @brief Resolves target fan indexes from a user-visible fan ID.
 *
 * fanId semantics:
 * -1 => all fans
 *  0..N-1 => one specific fan
 *
 * @param[in] fanId Fan selector.
 * @param[out] targetIndexes Resolved fan indexes.
 * @return ze_result_t ZE_RESULT_SUCCESS on success.
 */
ze_result_t fan::resolveTargetFanIndexes(int32_t fanId, std::vector<uint32_t> &targetIndexes)
{
	targetIndexes.clear();

	ze_result_t result = ensureFansEnumerated();
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	if (fanCount == 0) {
		return ZE_RESULT_ERROR_NOT_AVAILABLE;
	}

	if (fanId == -1) {
		targetIndexes.reserve(fanCount);
		for (uint32_t i = 0; i < fanCount; ++i) {
			targetIndexes.push_back(i);
		}
		return ZE_RESULT_SUCCESS;
	}

	if (fanId < 0 || static_cast<uint32_t>(fanId) >= fanCount) {
		ERR("Invalid fan ID {}. Valid values are -1 (all fans) or 0..{}\n", fanId, fanCount - 1);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	targetIndexes.push_back(static_cast<uint32_t>(fanId));
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Destructor for fan module.
 */
fan::~fan() { clearFanHandles(); }

/**
 * @brief Enumerates fan handles for the given device and caches them.
 *
 * @param[in] device Sysman device handle.
 * @return ze_result_t ZE_RESULT_SUCCESS on success.
 */
ze_result_t fan::enumFans(zes_device_handle_t device)
{
	deviceHandle = device;
	clearFanHandles();

	ze_result_t result = zesDeviceEnumFans(device, &fanCount, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to enumerate fans.\n");
		return result;
	}

	if (fanCount == 0) {
		DBG("No fans found on this device.\n");
		return ZE_RESULT_SUCCESS;
	}

	fanHandles = new zes_fan_handle_t[fanCount];
	result = zesDeviceEnumFans(device, &fanCount, fanHandles);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to retrieve fan handles.\n");
		clearFanHandles();
		return result;
	}

	fanPropsCache = new zes_fan_properties_t[fanCount]();
	fanPropsCached = new bool[fanCount]();
	fanConfigCache = new zes_fan_config_t[fanCount]();
	fanConfigCached = new bool[fanCount]();
	fanConfigCachedAtMs = new uint64_t[fanCount]();

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints supported fan speed control modes for debug logs.
 *
 * @param[in] mode Bitmask of supported fan modes.
 */
void fan::printSupportedModes(const uint32_t mode)
{
	DBG("    - Supported Modes: {}\n", mode);
	DBG("      - ");
	if ((mode & ZES_FAN_SPEED_MODE_DEFAULT) != 0)
		DBG("Default ");
	if ((mode & ZES_FAN_SPEED_MODE_FIXED) != 0)
		DBG("Fixed ");
	if (mode & ZES_FAN_SPEED_MODE_TABLE)
		DBG("Table ");
	DBG("\n");
}

/**
 * @brief Gets Level Zero fan properties for a handle.
 *
 * @param[in] fanHandle Fan handle.
 * @param[out] properties Populated property structure.
 * @return ze_result_t ZE_RESULT_SUCCESS on success.
 */
ze_result_t fan::getProperties(zes_fan_handle_t fanHandle, zes_fan_properties_t &properties)
{
	properties = {};
	ze_result_t result = zesFanGetProperties(fanHandle, &properties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get fan properties.\n");
		return result;
	}
	DBG("  - Fan Properties:\n");
	DBG("    - Fan SType: {}\n", properties.stype);
	DBG("    - Fan onSubdevice: {}\n", properties.onSubdevice);
	DBG("    - Fan Can Control: {}\n", properties.canControl);
	DBG("    - Fan Subdevice ID: {}\n", properties.subdeviceId);
	DBG("    - Fan Max Speed: {} RPM\n", properties.maxRPM);
	DBG("    - Fan Max Points: {}\n", properties.maxPoints);
	printSupportedModes(properties.supportedModes);

	return result;
}

/**
 * @brief Gets raw fan configuration for a selected fan ID.
 *
 * @param[out] config Populated fan config.
 * @param[in] fanId Fan selector (-1/all or specific fan ID).
 * @return ze_result_t ZE_RESULT_SUCCESS on success.
 */
ze_result_t fan::getConfig(zes_fan_config_t &config, int32_t fanId)
{
	std::vector<uint32_t> targets;
	ze_result_t result = resolveTargetFanIndexes(fanId, targets);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}
	if (targets.empty()) {
		return ZE_RESULT_ERROR_NOT_AVAILABLE;
	}

	const uint32_t selectedFanIndex = targets.front();
	return getConfigById(selectedFanIndex, config);
}

/**
 * @brief Returns number of enumerated fans.
 *
 * @param[out] count Number of fans.
 * @return ze_result_t ZE_RESULT_SUCCESS on success.
 */
ze_result_t fan::getFanCount(uint32_t &count)
{
	ze_result_t result = ensureFansEnumerated();
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}
	count = fanCount;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Initializes the fan module with device handle.
 *
 * @param[in] device Sysman device handle.
 * @return ze_result_t ZE_RESULT_SUCCESS.
 */
ze_result_t fan::init(zes_device_handle_t device)
{
	deviceHandle = device;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Runtime fan hook used by generic device run path.
 *
 * This implementation only ensures fan enumeration to keep init/runtime cost low.
 *
 * @param[in] device Sysman device handle.
 * @return ze_result_t ZE_RESULT_SUCCESS on success.
 */
ze_result_t fan::zesRun(zes_device_handle_t device)
{
	deviceHandle = device;
	return ensureFansEnumerated();
}

/**
 * @brief Reads fan speed percent for one fan ID.
 *
 * Falls back to RPM->percent conversion when direct percent state is unavailable.
 *
 * @param[in] fanId Fan index.
 * @param[out] pct Fan speed percentage.
 * @return ze_result_t ZE_RESULT_SUCCESS on success.
 */
ze_result_t fan::getSpeedPercentById(uint32_t fanId, int32_t &pct)
{
	ze_result_t result = ensureFansEnumerated();
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}
	if (fanHandles == nullptr || fanId >= fanCount) {
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// If a fixed target is configured, prefer reporting the configured setpoint
	// so config changes are immediately visible in stats output.
	zes_fan_config_t cfg = {};
	result = getConfigById(fanId, cfg);
	if (result == ZE_RESULT_SUCCESS) {
		if (cfg.mode == ZES_FAN_SPEED_MODE_FIXED) {
			if (cfg.speedFixed.units == ZES_FAN_SPEED_UNITS_PERCENT) {
				pct = cfg.speedFixed.speed;
				return ZE_RESULT_SUCCESS;
			}
			if (cfg.speedFixed.units == ZES_FAN_SPEED_UNITS_RPM) {
				zes_fan_properties_t props = {};
				if (getPropertiesById(fanId, props) == ZE_RESULT_SUCCESS && props.maxRPM > 0) {
					int32_t speedPct =
						static_cast<int32_t>((static_cast<int64_t>(cfg.speedFixed.speed) * 100) / props.maxRPM);
					pct = std::clamp(speedPct, 0, 100);
					return ZE_RESULT_SUCCESS;
				}
			}
		}

		if (cfg.mode == ZES_FAN_SPEED_MODE_TABLE && cfg.speedTable.numPoints > 0) {
			// Flat tables are used as fixed-speed fallback; expose the setpoint.
			bool flatTable = true;
			int32_t firstSpeed = cfg.speedTable.table[0].speed.speed;
			zes_fan_speed_units_t firstUnits = cfg.speedTable.table[0].speed.units;
			for (int32_t i = 1; i < cfg.speedTable.numPoints; ++i) {
				if (cfg.speedTable.table[i].speed.speed != firstSpeed ||
					cfg.speedTable.table[i].speed.units != firstUnits) {
					flatTable = false;
					break;
				}
			}
			if (flatTable) {
				if (firstUnits == ZES_FAN_SPEED_UNITS_PERCENT) {
					pct = firstSpeed;
					return ZE_RESULT_SUCCESS;
				}
				if (firstUnits == ZES_FAN_SPEED_UNITS_RPM) {
					zes_fan_properties_t props = {};
					if (getPropertiesById(fanId, props) == ZE_RESULT_SUCCESS && props.maxRPM > 0) {
						int32_t speedPct =
							static_cast<int32_t>((static_cast<int64_t>(firstSpeed) * 100) / props.maxRPM);
						pct = std::clamp(speedPct, 0, 100);
						return ZE_RESULT_SUCCESS;
					}
				}
			}
		}
	}

	int32_t speed = 0;
	result = zesFanGetState(fanHandles[fanId], ZES_FAN_SPEED_UNITS_PERCENT, &speed);
	if (result == ZE_RESULT_SUCCESS && speed >= 0) {
		pct = speed;
		return ZE_RESULT_SUCCESS;
	}

	int32_t speedRpm = 0;
	result = zesFanGetState(fanHandles[fanId], ZES_FAN_SPEED_UNITS_RPM, &speedRpm);
	if (result != ZE_RESULT_SUCCESS || speedRpm < 0) {
		return ZE_RESULT_ERROR_NOT_AVAILABLE;
	}

	zes_fan_properties_t props = {};
	result = getPropertiesById(fanId, props);
	if (result != ZE_RESULT_SUCCESS || props.maxRPM == 0) {
		return ZE_RESULT_ERROR_NOT_AVAILABLE;
	}

	int32_t speedPct = static_cast<int32_t>((static_cast<int64_t>(speedRpm) * 100) / props.maxRPM);
	if (speedPct < 0) {
		speedPct = 0;
	} else if (speedPct > 100) {
		speedPct = 100;
	}
	pct = speedPct;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Reads average fan speed percentage across all fans.
 *
 * @param[out] pct Average fan speed percent.
 * @return ze_result_t ZE_RESULT_SUCCESS on success.
 */
ze_result_t fan::getSpeedPercent(int32_t &pct)
{
	ze_result_t result = ensureFansEnumerated();
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}
	if (fanCount == 0 || fanHandles == nullptr) {
		return ZE_RESULT_ERROR_NOT_AVAILABLE;
	}

	int64_t totalPct = 0;
	uint32_t validCount = 0;
	for (uint32_t i = 0; i < fanCount; ++i) {
		int32_t speedPct = 0;
		ze_result_t res = getSpeedPercentById(i, speedPct);
		if (res == ZE_RESULT_SUCCESS && speedPct >= 0) {
			totalPct += speedPct;
			++validCount;
		}
	}
	if (validCount == 0) {
		return ZE_RESULT_ERROR_NOT_AVAILABLE;
	}
	pct = static_cast<int32_t>(totalPct / static_cast<int64_t>(validCount));
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Sets fixed fan speed in percent for selected fan target(s).
 *
 * @param[in] speedPercent Target speed percent.
 * @param[in] fanId Fan selector (-1 for all fans).
 * @return ze_result_t ZE_RESULT_SUCCESS on success.
 */
ze_result_t fan::setFixedSpeedPercent(int32_t speedPercent, int32_t fanId)
{
	std::vector<uint32_t> targets;
	ze_result_t resolveResult = resolveTargetFanIndexes(fanId, targets);
	if (resolveResult != ZE_RESULT_SUCCESS) {
		return resolveResult;
	}

	if (speedPercent < 0 || speedPercent > 100) {
		ERR("Invalid fan speed percentage: {}. Must be between 0 and 100.\n", speedPercent);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	zes_fan_speed_t speed;
	speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
	speed.speed = speedPercent;

	ze_result_t lastResult = ZE_RESULT_SUCCESS;
	for (uint32_t i : targets) {
		zes_fan_properties_t props = {};
		ze_result_t propRes = getPropertiesById(i, props);
		if (propRes == ZE_RESULT_SUCCESS && !props.canControl) {
			ERR("Fan {} does not support software speed control on this platform/firmware.\n", i);
			lastResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
			continue;
		}
		ze_result_t res = zesFanSetFixedSpeedMode(fanHandles[i], &speed);
		if ((res == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE || res == ZE_RESULT_ERROR_INVALID_ARGUMENT) &&
			propRes == ZE_RESULT_SUCCESS) {
			zes_fan_speed_table_t flatTable = {};
			flatTable.numPoints = 2;
			flatTable.table[0].temperature = 0;
			flatTable.table[0].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
			flatTable.table[0].speed.speed = speedPercent;
			flatTable.table[1].temperature = 100;
			flatTable.table[1].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
			flatTable.table[1].speed.speed = speedPercent;

			ERR("Fan {} rejected fixed mode; retrying with flat table mode at {}%.\n", i, speedPercent);
			res = zesFanSetSpeedTableMode(fanHandles[i], &flatTable);
		}
		if (res != ZE_RESULT_SUCCESS) {
			if (propRes == ZE_RESULT_SUCCESS) {
				ERR("Fan {} properties: canControl={}, supportedModes=0x{:X}, maxRPM={}\n", i, props.canControl,
					props.supportedModes, props.maxRPM);
			} else {
				ERR("Failed to query fan {} properties before set: 0x{:X} ({})\n", i, propRes,
					l0_error_to_string(propRes));
			}
			ERR("Failed to set fan {} to fixed speed mode ({} %). Error: {}\n", i, speedPercent, res);
			lastResult = res;
		} else {
			invalidateFanConfigCacheById(i);
		}
	}

	return lastResult;
}

/**
 * @brief Restores default fan control mode for selected fan target(s).
 *
 * @param[in] fanId Fan selector (-1 for all fans).
 * @return ze_result_t ZE_RESULT_SUCCESS on success.
 */
ze_result_t fan::setDefaultMode(int32_t fanId)
{
	std::vector<uint32_t> targets;
	ze_result_t resolveResult = resolveTargetFanIndexes(fanId, targets);
	if (resolveResult != ZE_RESULT_SUCCESS) {
		return resolveResult;
	}

	ze_result_t lastResult = ZE_RESULT_SUCCESS;
	for (uint32_t i : targets) {
		zes_fan_properties_t props = {};
		ze_result_t propRes = getPropertiesById(i, props);
		if (propRes == ZE_RESULT_SUCCESS && !props.canControl) {
			ERR("Fan {} does not support software speed control on this platform/firmware.\n", i);
			lastResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
			continue;
		}
		ze_result_t res = zesFanSetDefaultMode(fanHandles[i]);
		if (res != ZE_RESULT_SUCCESS) {
			if (propRes == ZE_RESULT_SUCCESS) {
				ERR("Fan {} properties: canControl={}, supportedModes=0x{:X}, maxRPM={}\n", i, props.canControl,
					props.supportedModes, props.maxRPM);
			} else {
				ERR("Failed to query fan {} properties before set: 0x{:X} ({})\n", i, propRes,
					l0_error_to_string(propRes));
			}
			ERR("Failed to set fan {} to default mode. Error: {}\n", i, res);
			lastResult = res;
		} else {
			invalidateFanConfigCacheById(i);
		}
	}

	return lastResult;
}

/**
 * @brief Sets fan table mode for selected fan target(s).
 *
 * @param[in] table Temperature/speed curve points.
 * @param[in] units Fan speed units (percent or RPM).
 * @param[in] fanId Fan selector (-1 for all fans).
 * @return ze_result_t ZE_RESULT_SUCCESS on success.
 */
ze_result_t fan::setSpeedTableMode(const std::vector<std::pair<uint32_t, int32_t>> &table, zes_fan_speed_units_t units,
								   int32_t fanId)
{
	std::vector<uint32_t> targets;
	ze_result_t resolveResult = resolveTargetFanIndexes(fanId, targets);
	if (resolveResult != ZE_RESULT_SUCCESS) {
		return resolveResult;
	}

	if (table.empty() || table.size() > ZES_FAN_TEMP_SPEED_PAIR_COUNT) {
		ERR("Invalid fan speed table point count: {}. Valid range: 1..{}\n", table.size(),
			ZES_FAN_TEMP_SPEED_PAIR_COUNT);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	zes_fan_speed_table_t speedTable = {};
	speedTable.numPoints = static_cast<int32_t>(table.size());

	for (size_t i = 0; i < table.size(); ++i) {
		speedTable.table[i].temperature = table[i].first;
		speedTable.table[i].speed.units = units;
		speedTable.table[i].speed.speed = table[i].second;
	}

	ze_result_t lastResult = ZE_RESULT_SUCCESS;
	for (uint32_t i : targets) {
		zes_fan_properties_t props = {};
		ze_result_t propRes = getPropertiesById(i, props);
		if (propRes == ZE_RESULT_SUCCESS && !props.canControl) {
			ERR("Fan {} does not support software speed control on this platform/firmware.\n", i);
			lastResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
			continue;
		}
		ze_result_t res = zesFanSetSpeedTableMode(fanHandles[i], &speedTable);
		if (res == ZE_RESULT_ERROR_INVALID_ARGUMENT && units == ZES_FAN_SPEED_UNITS_RPM &&
			propRes == ZE_RESULT_SUCCESS && props.maxRPM > 0) {
			zes_fan_speed_table_t pctTable = {};
			pctTable.numPoints = speedTable.numPoints;
			for (int32_t p = 0; p < pctTable.numPoints; ++p) {
				pctTable.table[p].temperature = speedTable.table[p].temperature;
				pctTable.table[p].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
				int64_t pct = (static_cast<int64_t>(speedTable.table[p].speed.speed) * 100) / props.maxRPM;
				if (pct < 0) {
					pct = 0;
				} else if (pct > 100) {
					pct = 100;
				}
				pctTable.table[p].speed.speed = static_cast<int32_t>(pct);
			}

			ERR("Fan {} rejected RPM table points; retrying fan curve with percent points.\n", i);
			res = zesFanSetSpeedTableMode(fanHandles[i], &pctTable);
		}
		if (res != ZE_RESULT_SUCCESS) {
			if (propRes == ZE_RESULT_SUCCESS) {
				ERR("Fan {} properties: canControl={}, supportedModes=0x{:X}, maxRPM={}\n", i, props.canControl,
					props.supportedModes, props.maxRPM);
			} else {
				ERR("Failed to query fan {} properties before set: 0x{:X} ({})\n", i, propRes,
					l0_error_to_string(propRes));
			}
			ERR("Failed to set fan {} speed table mode. Error: {}\n", i, res);
			lastResult = res;
		} else {
			invalidateFanConfigCacheById(i);
		}
	}

	return lastResult;
}
