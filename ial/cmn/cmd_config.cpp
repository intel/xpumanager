/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_config.h"
#include "debug.h"
#include <CLI/CLI.hpp>
#include "table_builder.h"
#include "bdf.h"
#include <assert.h>
#include <ecc.h>
#include <fabric.h>
#include <fan.h>
#include <ras.h>
#include <charconv>
#include <format>
#include <frequency.h>
#include <nlohmann/json.hpp>
#include <power.h>
#include <powerexp.h>
#include <scheduler.h>
#include <standby.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdlib>
#include <limits>

// Conversion helpers between watts and milliwatts
constexpr inline int mwToW(int32_t mw) { return static_cast<int>(mw / 1000); }
constexpr inline uint32_t wToMw(double w) { return static_cast<uint32_t>(w * 1000.0); }

// Fan curve input limits
constexpr uint32_t FAN_CURVE_TEMP_MIN_C = 0;
constexpr uint32_t FAN_CURVE_TEMP_MAX_C = 150;
constexpr int32_t FAN_CURVE_RPM_MIN = 0;
constexpr int32_t FAN_CURVE_RPM_MAX = 20000;

// Scheduler time limits in microseconds
constexpr uint64_t SCHEDULER_TIME_MIN = 5000;
constexpr uint64_t SCHEDULER_TIME_MAX = 100000000;

static std::unordered_map<configCmdType, configCmdStruct> configCmds = {
	{configCmdType::CONFIGHELP, {}},
	{configCmdType::CONFIGJSON, {}},
	{configCmdType::CONFIGDEVICE, {}},
	{configCmdType::TILE, {}},
	{configCmdType::FREQUENCYRANGE, {.func = &cmdConfig::setFrequencyRange}},
	{configCmdType::POWERLIMIT, {.func = &cmdConfig::setPowerLimit}},
	{configCmdType::STANDBYMODE, {.func = &cmdConfig::setStandby}},
	{configCmdType::SCHEDULERMODE, {.func = &cmdConfig::setScheduler}},
	{configCmdType::MEMORYECC, {.func = &cmdConfig::setMemoryEcc}},
	{configCmdType::PCIEDOWNGRADE, {.func = &cmdConfig::setPCIeGenUpdate}},
	{configCmdType::RESET, {.func = &cmdConfig::resetDevice}},
	{configCmdType::CLEARRAS, {.func = &cmdConfig::clearRasErrors}},
	{configCmdType::FANSPEED, {.func = &cmdConfig::setFanSpeed}},
	{configCmdType::FANCURVE, {.func = &cmdConfig::setFanCurve}},
	{configCmdType::FANCURVERPM, {.func = &cmdConfig::setFanCurveRpm}},
	{configCmdType::FANID, {}},
	{configCmdType::COLDRESET, {.func = &cmdConfig::coldResetDevice}},
	{configCmdType::IGNORE_GPU_USER_PROCESSES, {}},
	{configCmdType::FORCE_RESET_GPUS, {}},
	{configCmdType::POWERTYPE, {}},
};

/**
 * @brief Joins a vector of integers into a comma-separated string.
 *
 * @param[in] values Vector of integer values.
 * @return std::string Comma-separated list.
 */
static std::string joinUint32(const std::vector<uint32_t> &values)
{
	if (values.empty()) {
		return "";
	}
	std::ostringstream oss;
	for (size_t i = 0; i < values.size(); ++i) {
		oss << values[i];
		if (i + 1 < values.size()) {
			oss << ", ";
		}
	}
	return oss.str();
}

/**
 * @brief Joins frequency values into a rounded, deduplicated list.
 *
 * @param[in] values Frequency values in MHz.
 * @return std::string Comma-separated list of rounded frequencies.
 */
static std::string joinRoundedClocks(const std::vector<double> &values)
{
	if (values.empty()) {
		return "";
	}
	std::vector<uint32_t> rounded;
	rounded.reserve(values.size());
	for (double v : values) {
		if (v < 0) {
			continue;
		}
		rounded.push_back(static_cast<uint32_t>(std::llround(v)));
	}
	std::sort(rounded.begin(), rounded.end());
	rounded.erase(std::unique(rounded.begin(), rounded.end()), rounded.end());
	return joinUint32(rounded);
}

/**
 * @brief Gets available GPU frequency options and range for a tile.
 *
 * @param[in] d Device information.
 * @param[in] tileId Tile identifier.
 * @param[out] options Comma-separated frequency list.
 * @param[out] minFreq Minimum frequency in MHz.
 * @param[out] maxFreq Maximum frequency in MHz.
 */
static void getGpuFrequencyOptions(devInfo *d, uint32_t tileId, std::string &options, uint32_t &minFreq,
								   uint32_t &maxFreq)
{
	options.clear();
	minFreq = 0;
	maxFreq = 0;

	frequency *fq = d->dev->getFrequency();
	if (fq == nullptr) {
		return;
	}

	std::vector<double> clocks;
	ze_result_t result = fq->getFreqAvailableClocks(tileId, clocks);
	if (result != ZE_RESULT_SUCCESS || clocks.empty()) {
		return;
	}

	options = joinRoundedClocks(clocks);

	double minVal = *std::min_element(clocks.begin(), clocks.end());
	double maxVal = *std::max_element(clocks.begin(), clocks.end());

	// Read the configured frequency range (not the hardware capability range)
	double configuredMin = 0;
	double configuredMax = 0;
	result = fq->getFreqRangeForTile(tileId, configuredMin, configuredMax);
	if (result == ZE_RESULT_SUCCESS) {
		minFreq =
			static_cast<uint32_t>(std::llround((std::fpclassify(configuredMin) != FP_ZERO) ? configuredMin : minVal));
		maxFreq =
			static_cast<uint32_t>(std::llround((std::fpclassify(configuredMax) != FP_ZERO) ? configuredMax : maxVal));
	} else {
		// Fallback to available clocks range only if configured range could not be read
		minFreq = static_cast<uint32_t>(std::llround(minVal));
		maxFreq = static_cast<uint32_t>(std::llround(maxVal));
	}

	if (minFreq > maxFreq) {
		std::swap(minFreq, maxFreq);
	}
}

/**
 * @brief Maps a tile id to a subdevice id if supported.
 *
 * @param[in] d Device information.
 * @param[in] tileId Tile identifier.
 * @return uint32_t Subdevice identifier (or tileId if mapping unavailable).
 */
static uint32_t mapTileToSubdevice(devInfo *d, uint32_t tileId)
{
	zes_device_properties_t props = {};
	props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
	if (d->dev->zesGetDevProps(d->zesDeviceHdl, &props) == ZE_RESULT_SUCCESS) {
		if (props.numSubdevices == 0) {
			return tileId;
		}
	}
	zes_subdevice_exp_properties_t subdeviceProps = {};
	ze_result_t result = d->dev->getSubdeviceProperties(tileId, subdeviceProps);
	if (result == ZE_RESULT_SUCCESS) {
		return subdeviceProps.subdeviceId;
	}
	return tileId;
}

/**
 * @brief Gets scheduler mode and parameters for a tile.
 *
 * @param[in] d Device information.
 * @param[in] tileId Tile identifier.
 * @param[out] mode Scheduler mode string.
 * @param[out] interval Timeslice/timeout interval in microseconds.
 * @param[out] yieldTimeout Timeslice yield timeout in microseconds.
 */
static void getSchedulerInfo(devInfo *d, uint32_t tileId, std::string &mode, uint64_t &interval, uint64_t &yieldTimeout)
{
	mode.clear();
	interval = 0;
	yieldTimeout = 0;

	scheduler *sched = d->dev->getScheduler();
	if (sched == nullptr) {
		return;
	}

	uint32_t schedCount = sched->getSchedulerCount();
	zes_sched_handle_t *schedHandles = sched->getSchedulerHandles();
	if (schedCount == 0 || schedHandles == nullptr) {
		return;
	}

	uint32_t targetSubdeviceId = mapTileToSubdevice(d, tileId);
	zes_sched_handle_t selected = nullptr;

	for (uint32_t i = 0; i < schedCount; ++i) {
		zes_sched_properties_t props = {};
		if (sched->getProperties(schedHandles[i], &props) != ZE_RESULT_SUCCESS) {
			continue;
		}
		if (props.onSubdevice) {
			if (props.subdeviceId == targetSubdeviceId) {
				selected = schedHandles[i];
				break;
			}
		} else if (tileId == 0) {
			selected = schedHandles[i];
			break;
		}
	}

	if (selected == nullptr) {
		return;
	}

	zes_sched_mode_t schedMode = {};
	if (sched->getCurrentMode(selected, &schedMode) != ZE_RESULT_SUCCESS) {
		return;
	}

	if (schedMode == ZES_SCHED_MODE_TIMEOUT) {
		mode = "timeout";
		zes_sched_timeout_properties_t timeoutProps = {};
		if (sched->getTimeoutModeProperties(selected, false, &timeoutProps) == ZE_RESULT_SUCCESS) {
			interval = timeoutProps.watchdogTimeout;
		}
	} else if (schedMode == ZES_SCHED_MODE_TIMESLICE) {
		mode = "timeslice";
		zes_sched_timeslice_properties_t timesliceProps = {};
		if (sched->getTimesliceProperties(selected, false, &timesliceProps) == ZE_RESULT_SUCCESS) {
			interval = timesliceProps.interval;
			yieldTimeout = timesliceProps.yieldTimeout;
		}
	} else if (schedMode == ZES_SCHED_MODE_EXCLUSIVE) {
		mode = "exclusive";
	} else {
		mode = "unknown";
	}
}

/**
 * @brief Converts ECC state enum to user-friendly text.
 *
 * @param[in] state ECC state value.
 * @return std::string "enabled", "disabled", or "N/A".
 */
static std::string eccStateToString(zes_device_ecc_state_t state)
{
	if (state == ZES_DEVICE_ECC_STATE_ENABLED) {
		return "enabled";
	}

	if (state == ZES_DEVICE_ECC_STATE_DISABLED) {
		return "disabled";
	}

	return "N/A";
}

/**
 * @brief Gets current and pending memory ECC states for a device.
 *
 * @param[in] d Device information.
 * @param[out] current Current ECC state string.
 * @param[out] pending Pending ECC state string.
 */
static void getMemoryEccStates(devInfo *d, std::string &current, std::string &pending)
{
	current = "N/A";
	pending = "N/A";

	ecc *eccInst = d->dev->getECC();
	if (eccInst != nullptr) {
		zes_device_ecc_properties_t state = {};
		if (eccInst->getState(d->zesDeviceHdl, &state) == ZE_RESULT_SUCCESS) {
			current = eccStateToString(state.currentState);
			pending = eccStateToString(state.pendingState);
			return;
		}
	}

	auto zeDevProp = ze_device_properties_t{};
	if (d->dev->getDevProps(d->deviceHdl, &zeDevProp) == ZE_RESULT_SUCCESS) {
		const std::string fallback = (zeDevProp.flags & ZE_DEVICE_PROPERTY_FLAG_ECC) ? "enabled" : "disabled";
		current = fallback;
		pending = fallback;
	}
}

/**
 * @brief Gets standby mode for a tile.
 *
 * @param[in] d Device information.
 * @param[in] tileId Tile identifier.
 * @return std::string Standby mode string.
 */
static std::string getStandbyMode(devInfo *d, uint32_t tileId)
{
	standby *sb = d->dev->getStandby();
	if (sb == nullptr) {
		return "";
	}

	uint32_t standbyCount = sb->getStandbyCount();
	zes_standby_handle_t *standbyHandles = sb->getStandbyHandles();
	if (standbyCount == 0 || standbyHandles == nullptr) {
		return "";
	}

	uint32_t targetSubdeviceId = mapTileToSubdevice(d, tileId);
	zes_standby_handle_t selected = nullptr;

	for (uint32_t i = 0; i < standbyCount; ++i) {
		zes_standby_properties_t props = {};
		if (sb->getProperties(standbyHandles[i], &props) != ZE_RESULT_SUCCESS) {
			continue;
		}
		if (props.onSubdevice) {
			if (props.subdeviceId == targetSubdeviceId) {
				selected = standbyHandles[i];
				break;
			}
		} else if (tileId == 0) {
			selected = standbyHandles[i];
			break;
		}
	}

	if (selected == nullptr) {
		return "";
	}

	zes_standby_promo_mode_t mode = {};
	if (sb->getMode(selected, &mode) != ZE_RESULT_SUCCESS) {
		return "";
	}

	if (mode == ZES_STANDBY_PROMO_MODE_DEFAULT) {
		return "default";
	}
	if (mode == ZES_STANDBY_PROMO_MODE_NEVER) {
		return "never";
	}
	return "";
}

/**
 * @brief Builds JSON string for device configuration output.
 *
 * @param[in] d Device information.
 * @param[in] indent Base indentation size.
 * @return std::string JSON-formatted configuration.
 */
static std::string buildDeviceConfigJson(devInfo *d, size_t indent)
{
	std::string eccCurrent;
	std::string eccPending;
	getMemoryEccStates(d, eccCurrent, eccPending);

	int plPackageSustain = 0;
	int plPackageBurst = 0;
	int plPackagePeak = 0;
	int plCardSustain = 0;
	int plCardBurst = 0;
	int plCardPeak = 0;
	std::string powerValidRange;
	power *pwr = d->dev->getPower();
	powerExp *pwrExp = d->dev->getPowerExp();
	if (!pwrExp->isPowerExpEnabled() && pwr != nullptr) {
		std::map<zes_power_domain_t, std::map<zes_power_level_t, int32_t>> domainLimits;
		pwr->getDomainLimits(domainLimits);

		auto getLimitW = [&](zes_power_domain_t domain, zes_power_level_t level) -> int {
			auto domainIt = domainLimits.find(domain);
			if (domainIt == domainLimits.end()) {
				return 0;
			}
			auto levelIt = domainIt->second.find(level);
			if (levelIt == domainIt->second.end()) {
				return 0;
			}
			return mwToW(levelIt->second);
		};

		plCardSustain = getLimitW(ZES_POWER_DOMAIN_CARD, ZES_POWER_LEVEL_SUSTAINED);
		plCardBurst = getLimitW(ZES_POWER_DOMAIN_CARD, ZES_POWER_LEVEL_BURST);
		plCardPeak = getLimitW(ZES_POWER_DOMAIN_CARD, ZES_POWER_LEVEL_PEAK);
		plPackageSustain = getLimitW(ZES_POWER_DOMAIN_PACKAGE, ZES_POWER_LEVEL_SUSTAINED);
		plPackageBurst = getLimitW(ZES_POWER_DOMAIN_PACKAGE, ZES_POWER_LEVEL_BURST);
		plPackagePeak = getLimitW(ZES_POWER_DOMAIN_PACKAGE, ZES_POWER_LEVEL_PEAK);

		pwr->getMaxPowerLimit(powerValidRange);
	} else if (pwrExp->isPowerExpEnabled()) {
		std::vector<power_limits_exp_t> powerLimits;
		if (pwrExp->getPowerLimits(powerLimits) == ZE_RESULT_SUCCESS) {
			for (const auto &limits : powerLimits) {
				if (limits.domain == ZES_POWER_DOMAIN_CARD) {
					plCardSustain = mwToW(limits.limit);
				} else if (limits.domain == ZES_POWER_DOMAIN_PACKAGE) {
					plPackageSustain = mwToW(limits.limit);
				}
			}
		}
	}

	uint32_t tileCount = 0;
	zes_device_properties_t props = {};
	props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
	if (d->dev->zesGetDevProps(d->zesDeviceHdl, &props) == ZE_RESULT_SUCCESS) {
		tileCount = props.numSubdevices;
	}
	if (tileCount == 0) {
		tileCount = 1;
	}

	nlohmann::ordered_json deviceJson;
	deviceJson["device_id"] = d->index;
	deviceJson["memory_ecc_current_state"] = eccCurrent;
	deviceJson["memory_ecc_pending_state"] = eccPending;
	deviceJson["pl_card_sustain"] = plCardSustain;
	deviceJson["pl_card_burst"] = plCardBurst;
	deviceJson["pl_card_peak"] = plCardPeak;
	deviceJson["pl_package_sustain"] = plPackageSustain;
	deviceJson["pl_package_burst"] = plPackageBurst;
	deviceJson["pl_package_peak"] = plPackagePeak;
	deviceJson["power_valid_range"] = powerValidRange;

	// Fan config — per-fan index
	{
		auto *fanHandler = reinterpret_cast<::fan *>(d->dev->getFan());
		nlohmann::ordered_json fansArray = nlohmann::ordered_json::array();
		if (fanHandler != nullptr) {
			uint32_t fanCount = 0;
			if (fanHandler->getFanCount(fanCount) == ZE_RESULT_SUCCESS) {
				for (uint32_t fanId = 0; fanId < fanCount; ++fanId) {
					nlohmann::ordered_json fanEntry;
					fanEntry["id"] = fanId;
					zes_fan_config_t fanCfg = {};
					if (fanHandler->getConfig(fanCfg, static_cast<int32_t>(fanId)) == ZE_RESULT_SUCCESS) {
						if (fanCfg.mode == ZES_FAN_SPEED_MODE_DEFAULT) {
							fanEntry["mode"] = "default";
						} else if (fanCfg.mode == ZES_FAN_SPEED_MODE_FIXED) {
							fanEntry["mode"] = "fixed";
							fanEntry["fixed_speed"] = fanCfg.speedFixed.speed;
							fanEntry["fixed_speed_units"] =
								(fanCfg.speedFixed.units == ZES_FAN_SPEED_UNITS_PERCENT) ? "percent" : "rpm";
						} else if (fanCfg.mode == ZES_FAN_SPEED_MODE_TABLE) {
							fanEntry["mode"] = "table";
						} else {
							fanEntry["mode"] = "N/A";
						}
					} else {
						fanEntry["mode"] = "N/A";
					}
					int32_t speedPct = -1;
					if (fanHandler->getSpeedPercentById(fanId, speedPct) == ZE_RESULT_SUCCESS && speedPct >= 0) {
						fanEntry["speed_percent"] = speedPct;
					}
					fansArray.push_back(fanEntry);
				}
			}
		}
		deviceJson["fans"] = fansArray;
	}

	nlohmann::ordered_json tileConfigArray = nlohmann::ordered_json::array();

	for (uint32_t tileId = 0; tileId < tileCount; ++tileId) {
		std::string tileIdStr = std::to_string(d->index) + "/" + std::to_string(tileId);

		std::string freqOptions;
		uint32_t minFreq = 0;
		uint32_t maxFreq = 0;
		getGpuFrequencyOptions(d, tileId, freqOptions, minFreq, maxFreq);

		std::string schedulerMode;
		uint64_t schedInterval = 0;
		uint64_t schedYieldTimeout = 0;
		getSchedulerInfo(d, tileId, schedulerMode, schedInterval, schedYieldTimeout);

		std::string standbyMode = getStandbyMode(d, tileId);

		nlohmann::ordered_json tileJson;
		tileJson["compute_engine"] = "compute";
		tileJson["gpu_frequency_valid_options"] = freqOptions;
		tileJson["max_frequency"] = maxFreq;
		tileJson["media_engine"] = "media";
		tileJson["min_frequency"] = minFreq;
		tileJson["scheduler_mode"] = schedulerMode;
		uint64_t outInterval = (schedulerMode == "timeslice") ? schedInterval : 0;
		uint64_t outYieldTimeout = (schedulerMode == "timeslice") ? schedYieldTimeout : 0;
		tileJson["scheduler_timeslice_interval"] = outInterval;
		tileJson["scheduler_timeslice_yield_timeout"] = outYieldTimeout;
		tileJson["standby_mode"] = standbyMode;
		tileJson["standby_mode_valid_options"] = "default, never";
		tileJson["tile_id"] = tileIdStr;

		tileConfigArray.push_back(tileJson);
	}

	deviceJson["tile_config_data"] = tileConfigArray;

	std::string jsonOut = deviceJson.dump(4);
	if (indent == 0) {
		return jsonOut;
	}

	std::string indentStr(indent, ' ');
	std::ostringstream out;
	std::istringstream in(jsonOut);
	std::string line;
	while (std::getline(in, line)) {
		out << indentStr << line;
		if (!in.eof()) {
			out << "\n";
		}
	}
	return out.str();
}

/**
 * @brief Splits a string into tokens based on a delimiter.
 *
 * @param[in] str The string to split.
 * @param[in] delimiter The character to use as delimiter.
 *
 * @return std::vector<std::string> Vector containing the split tokens.
 */
static std::vector<std::string> split(const std::string &str, char delimiter)
{
	std::vector<std::string> tokens;
	std::stringstream ss(str);
	std::string token;
	while (std::getline(ss, token, delimiter)) {
		tokens.push_back(token);
	}
	return tokens;
}

/**
 * @brief Parses an unsigned integer from a string without throwing exceptions.
 *
 * @param[in] input String to parse.
 * @param[out] value Parsed uint32_t value.
 * @return true if parsing succeeds and consumes the full string; false otherwise.
 */
static bool parseUint32NoThrow(const std::string &input, uint32_t &value)
{
	auto result = std::from_chars(input.data(), input.data() + input.size(), value);
	return result.ec == std::errc() && result.ptr == input.data() + input.size();
}

/**
 * @brief Parses optional fan ID selection from command options.
 *
 * Supports -1 (all fans) or 0..N-1 (single fan).
 *
 * @param[out] fanId Parsed fan target ID.
 * @return ze_result_t ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdConfig::getSelectedFanId(int32_t &fanId)
{
	fanId = -1;
	if (!configCmds[configCmdType::FANID].enabled) {
		return ZE_RESULT_SUCCESS;
	}

	const std::string &fanIdStr = configCmds[configCmdType::FANID].val;
	if (fanIdStr.empty()) {
		ERR("Error: --fanid cannot be empty. Use -1 for all fans, or 0..N-1 for a single fan.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	try {
		size_t pos = 0;
		long long parsed = std::stoll(fanIdStr, &pos);
		if (pos != fanIdStr.size() || parsed < -1 || parsed > std::numeric_limits<int32_t>::max()) {
			ERR("Error: invalid --fanid '{}'. Use -1 for all fans, or 0..N-1 for a single fan.\n", fanIdStr.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		fanId = static_cast<int32_t>(parsed);
		return ZE_RESULT_SUCCESS;
	} catch (const std::exception &) {
		ERR("Error: invalid --fanid '{}'. Use -1 for all fans, or 0..N-1 for a single fan.\n", fanIdStr.c_str());
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
}

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdConfig::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Get and change the GPU settings"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s config [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s config --device [deviceId]", progName.c_str()));
	helpList.push_back(
		helpCmd(HEADING, "%s config --device [deviceId] [--tile tileId] --frequencyrange [minFrequency,maxFrequency]",
				progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s config --device [deviceId] --powerlimit [powerValue]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s config --device [deviceId] --standby [standbyMode]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s config --device [deviceId] [--tile tileId] --scheduler [schedulerMode]",
							   progName.c_str()));
	helpList.push_back(
		helpCmd(HEADING, "%s config --device [deviceId] --memoryecc [0|1] 0:disable; 1:enable", progName.c_str()));
	helpList.push_back(
		helpCmd(HEADING, "%s config --device [deviceId] --pciedowngrade [0|1] 0:disable; 1:enable", progName.c_str()));
	helpList.push_back(
		helpCmd(HEADING, "%s config --device [deviceId] [--fanid <id|-1>] --fanspeed [value] 0-100(percent);default",
				progName.c_str()));
	helpList.push_back(helpCmd(
		HEADING,
		"%s config --device [deviceId] [--fanid <id|-1>] --fancurve [curve] temp:speed pairs, e.g. 40:25,60:50,80:90",
		progName.c_str()));
	helpList.push_back(helpCmd(
		HEADING,
		"%s config --device [deviceId] [--fanid <id|-1>] --fancurve-rpm [curve] temp:rpm pairs, e.g. 40:1200,70:2800",
		progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "--fanid                     Fan target: -1 (all fans, default) or 0..N-1"));

	helpList.push_back(helpCmd(HEADING, "%s config --device [deviceId] --reset", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s config --device [pciBdfAddress] --coldreset", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s config --device [deviceId] --clear-ras-errors", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "--device,--id               The device ID or PCI BDF address to query"));
	helpList.push_back(helpCmd(HEADING, "-t,--tile                   The tile ID"));
	helpList.push_back(helpCmd(
		HEADING, "--frequencyrange            Core frequency range (MHz). Applies to all tiles when -t is omitted"));
	helpList.push_back(helpCmd(HEADING, "--powerlimit                Device-level power limit"));
	helpList.push_back(helpCmd(
		HEADING, "--standby                   Standby mode (device-level). Valid options: \"default\"; \"never\""));
	helpList.push_back(helpCmd(
		HEADING,
		"--scheduler                 Scheduler mode. Applies to all tiles when -t is omitted. "
		"Value options: \"timeout\",timeoutValue (us); \"timeslice\",interval (us),yieldtimeout (us);\"exclusive\""));
	helpList.push_back(helpCmd(SUB_HEADING, "The valid range of all time values (us) is from 5000 to 100,000,000."));
	helpList.push_back(helpCmd(HEADING, "--reset                     Reset device by SBR (Secondary Bus Reset)"));
	helpList.push_back(helpCmd(SUB_HEADING, "For Intel(R) Max Series GPU, when SR-IOV is enabled, please add "
											"\"pci=realloc=off\" into Linux kernel command line parameters"));
	helpList.push_back(
		helpCmd(SUB_HEADING,
				"When SR-IOV is disabled, please add \"pci=realloc=on\" into Linux kernel command line parameters"));
	helpList.push_back(helpCmd(HEADING, "--coldreset                 Cold reset device via PCIe slot power cycle"));
	helpList.push_back(helpCmd(SUB_HEADING, "Device must be addressed by PCI BDF (e.g. 0000:4d:00.0), not by device "
											"ID. Aborts if processes are using the GPU or if other devices share "
											"the PCIe slot. Override with the flags below."));
	helpList.push_back(
		helpCmd(HEADING, "--ignore-gpu-user-processes Proceed with --coldreset even if processes have the GPU"));
	helpList.push_back(helpCmd(SUB_HEADING, "open."));
	helpList.push_back(
		helpCmd(HEADING, "--force-reset-gpus          Proceed with --coldreset even if other PCI devices share"));
	helpList.push_back(helpCmd(SUB_HEADING, "the PCIe slot. All devices on the slot will be reset together."));
	helpList.push_back(helpCmd(HEADING, "--clear-ras-errors          Clear all RAS error counters for the device"));
	helpList.push_back(
		helpCmd(HEADING, "--memoryecc                 Enable/disable memory ECC setting. 0:disable; 1:enable"));
	helpList.push_back(helpCmd(HEADING, "--powerlimit                Device-level power limit"));
	helpList.push_back(helpCmd(
		HEADING,
		"--powertype                 Device-level power limit type. Valid options: \"sustain\"; \"peak\"; \"burst\""));
	helpList.push_back(
		helpCmd(HEADING, "--pciedowngrade                 Enable/disable PCIe downgrade setting. 0:disable; 1:enable"));
	helpList.push_back(helpCmd(HEADING, "--fanspeed                  Set fan speed percentage: 0-100; default (auto)"));
	helpList.push_back(helpCmd(
		SUB_HEADING, "Examples: --fanspeed 50   --fanspeed 50%%   --fanspeed default   --fanid 1 --fanspeed 50"));
	helpList.push_back(
		helpCmd(HEADING, "--fancurve                  Set fan curve in percent: temp:speed,temp:speed,..."));
	helpList.push_back(
		helpCmd(SUB_HEADING, "Examples: --fancurve 40:25,60:50,80:90   --fancurve 40:25%,60:50%,80:90%"));
	helpList.push_back(helpCmd(HEADING, "--fancurve-rpm              Set fan curve in RPM: temp:rpm,temp:rpm,..."));
	helpList.push_back(
		helpCmd(SUB_HEADING, "Examples: --fancurve-rpm 40:1200,70:2800   --fancurve-rpm 40:1200rpm,70:2800rpm"));

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Displays the current device configuration including power, ECC, and tile-level settings.
 *
 * This function queries and displays comprehensive device configuration information including:
 * - Power limits (sustained, burst, peak, instantaneous)
 * - Memory ECC state (current and pending)
 * - Tile-level configurations (frequency, standby, scheduler)
 *
 * @param[in] d Device information structure containing device handle and properties.
 */
void cmdConfig::displayDeviceConfig(devInfo *d)
{
	TRACING();

	TableBuilder table;
	table.addColumn("Device Type", 11);
	table.addColumn("Device ID/Tile ID", 17);
	table.addColumn("Configuration", 62);

	// Power configuration - using getDomainLimits() via HAL
	std::map<zes_power_domain_t, std::map<zes_power_level_t, int32_t>> domainLimits;
	power *pwr = d->dev->getPower();
	if (pwr != nullptr) {
		pwr->getDomainLimits(domainLimits);
	}

	// Display power limits by domain (fixed order: card, package)
	auto addPowerDomain = [&](bool showDeviceId, const char *domainStr,
							  const std::map<zes_power_level_t, int32_t> *limits) {
		std::string domainLabel = " Power domain " + std::string(domainStr) + ":";
		if (showDeviceId) {
			table.addRow("GPU", std::to_string(d->index), domainLabel);
		} else {
			table.addRow("", "", domainLabel);
		}

		auto getLimitStr = [&](zes_power_level_t level) -> std::string {
			if (limits != nullptr) {
				auto it = limits->find(level);
				if (it != limits->end()) {
					return std::to_string(mwToW(it->second));
				}
			}
			return "N/A";
		};

		table.addRow("", "", "  sustain(w): " + getLimitStr(ZES_POWER_LEVEL_SUSTAINED));
		table.addRow("", "", "  burst(w): " + getLimitStr(ZES_POWER_LEVEL_BURST));
		table.addRow("", "", "  peak(w): " + getLimitStr(ZES_POWER_LEVEL_PEAK));
	};

	const std::map<zes_power_level_t, int32_t> *cardLimits = nullptr;
	const std::map<zes_power_level_t, int32_t> *packageLimits = nullptr;
	auto cardIt = domainLimits.find(ZES_POWER_DOMAIN_CARD);
	if (cardIt != domainLimits.end()) {
		cardLimits = &cardIt->second;
	}
	auto packageIt = domainLimits.find(ZES_POWER_DOMAIN_PACKAGE);
	if (packageIt != domainLimits.end()) {
		packageLimits = &packageIt->second;
	}

	addPowerDomain(true, "card", cardLimits);
	addPowerDomain(false, "package", packageLimits);

	// Memory ECC configuration (avoid logging errors on unsupported devices)
	table.addRow("", "", "");
	table.addRow("", "", " Memory ECC:");
	std::string eccCurrent;
	std::string eccPending;
	getMemoryEccStates(d, eccCurrent, eccPending);
	table.addRow("", "", "  Current: " + eccCurrent);
	table.addRow("", "", "  Pending: " + eccPending);

	// PCIe Gen4 Downgrade (if available - this may not be available on all platforms)
	// TODO: Check if there's a sysman API for this
	table.addRow("", "", "");
	table.addRow("", "", " PCIe Gen4 Downgrade:");
	table.addRow("", "", "  Current: N/A");

	// Fan configuration (device-level)
	table.addRow("", "", "");
	table.addRow("", "", " Fan (Mode / Speed %):");
	auto *fanHandler = reinterpret_cast<::fan *>(d->dev->getFan());
	if (fanHandler == nullptr) {
		table.addRow("", "", "  Current: N/A");
	} else {
		uint32_t fanCount = 0;
		ze_result_t fanCountResult = fanHandler->getFanCount(fanCount);
		if (fanCountResult != ZE_RESULT_SUCCESS || fanCount == 0) {
			table.addRow("", "", "  Current: N/A");
		} else {
			for (uint32_t fanId = 0; fanId < fanCount; ++fanId) {
				std::string modeText = "N/A";
				zes_fan_config_t fanCfg = {};
				ze_result_t fanResult = fanHandler->getConfig(fanCfg, static_cast<int32_t>(fanId));
				if (fanResult == ZE_RESULT_SUCCESS) {
					if (fanCfg.mode == ZES_FAN_SPEED_MODE_DEFAULT) {
						modeText = "Default";
					} else if (fanCfg.mode == ZES_FAN_SPEED_MODE_FIXED) {
						modeText = "Fixed";
					} else if (fanCfg.mode == ZES_FAN_SPEED_MODE_TABLE) {
						modeText = "Table";
					}
				}

				int32_t speedPct = -1;
				ze_result_t speedResult = fanHandler->getSpeedPercentById(fanId, speedPct);
				if (speedResult == ZE_RESULT_SUCCESS && speedPct >= 0) {
					table.addRow("", "", std::format("  Fan {}: {} / {}%", fanId, modeText, speedPct));
				} else {
					table.addRow("", "", std::format("  Fan {}: {} / N/A", fanId, modeText));
				}
			}
		}
	}

	table.addSeparator();

	// Display tile-level configuration
	uint32_t tileCount = 0;
	zes_device_properties_t devProps = {};
	devProps.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
	if (d->dev->zesGetDevProps(d->zesDeviceHdl, &devProps) == ZE_RESULT_SUCCESS) {
		tileCount = devProps.numSubdevices;
	}

	// If no subdevices, show device-level config
	if (tileCount == 0) {
		tileCount = 1;
	}

	for (uint32_t tileId = 0; tileId < tileCount; tileId++) {
		std::string tileIdStr = std::format("{}/{}", d->index, tileId);

		// GPU Frequency
		frequency *fq = d->dev->getFrequency();
		if (fq != nullptr) {
			std::vector<double> clocks;
			double minFreq = 0, maxFreq = 0;

			// Try to get frequency range and available clocks for this tile via HAL
			bool gotClocks = false;
			ze_result_t result = fq->getFreqAvailableClocks(tileId, clocks);
			if (result == ZE_RESULT_SUCCESS && !clocks.empty()) {
				gotClocks = true;
			}

			// Get current range via HAL
			fq->getFreqRangeForTile(tileId, minFreq, maxFreq);

			if (minFreq > 0 || maxFreq > 0) {
				table.addRow("GPU", tileIdStr, std::format(" GPU Min Frequency (MHz): {:.0f}", minFreq));
				table.addRow("", "", std::format(" GPU Max Frequency (MHz): {:.0f}", maxFreq));
			} else {
				table.addRow("GPU", tileIdStr, " GPU Min Frequency (MHz): N/A");
				table.addRow("", "", " GPU Max Frequency (MHz): N/A");
			}

			// Display valid options
			const int firstLineBreakWidth = 43;
			const int firstContinuationBreakWidth = 55;
			const int nextLineBreakWidth = 52;
			if (gotClocks && !clocks.empty()) {
				bool firstLine = true;
				int continuationLineIndex = 0;
				std::string currentLine;

				for (size_t i = 0; i < clocks.size(); i++) {
					std::string token = std::to_string(static_cast<int>(clocks[i]));
					std::string toAdd = currentLine.empty() ? token : ", " + token;
					int maxWidth =
						firstLine ? firstLineBreakWidth
								  : (continuationLineIndex == 0 ? firstContinuationBreakWidth : nextLineBreakWidth);
					if (static_cast<int>(currentLine.length() + toAdd.length()) > maxWidth) {
						std::string lineOut = currentLine;
						if (!lineOut.empty() && i < clocks.size()) {
							lineOut += ",";
						}
						if (firstLine) {
							table.addRow("", "", "  Valid Options: " + lineOut);
						} else {
							table.addRow("", "", "    " + lineOut);
						}
						if (firstLine) {
							firstLine = false;
						} else {
							continuationLineIndex++;
						}
						currentLine = token;
					} else {
						currentLine += toAdd;
					}
				}

				if (!currentLine.empty()) {
					if (firstLine) {
						table.addRow("", "", "  Valid Options: " + currentLine);
					} else {
						table.addRow("", "", "    " + currentLine);
					}
				}
			} else {
				table.addRow("", "", "  Valid Options: N/A");
			}
		}

		table.addRow("", "", "");

		// Standby Mode
		standby *sb = d->dev->getStandby();
		if (sb != nullptr) {
			std::string standbyModeStr = getStandbyMode(d, tileId);
			if (!standbyModeStr.empty()) {
				table.addRow("", "", std::format(" Standby Mode: {}", standbyModeStr));
			} else {
				table.addRow("", "", " Standby Mode: N/A");
			}
		}
		table.addRow("", "", "  Valid Options: default, never");

		table.addRow("", "", "");

		// Scheduler Mode
		scheduler *sched = d->dev->getScheduler();
		if (sched != nullptr) {
			uint32_t schedCount = sched->getSchedulerCount();
			zes_sched_handle_t *schedHandles = sched->getSchedulerHandles();
			if (schedCount > 0 && schedHandles != nullptr) {
				for (uint32_t i = 0; i < schedCount; i++) {
					zes_sched_properties_t schedProps = {};
					if (sched->getProperties(schedHandles[i], &schedProps) == ZE_RESULT_SUCCESS) {
						if (!schedProps.onSubdevice || schedProps.subdeviceId == tileId) {
							zes_sched_mode_t mode = {};
							if (sched->getCurrentMode(schedHandles[i], &mode) == ZE_RESULT_SUCCESS) {
								const char *modeStr = "unknown";
								switch (mode) {
								case ZES_SCHED_MODE_TIMEOUT:
									modeStr = "timeout";
									break;
								case ZES_SCHED_MODE_TIMESLICE:
									modeStr = "timeslice";
									break;
								case ZES_SCHED_MODE_EXCLUSIVE:
									modeStr = "exclusive";
									break;
								case ZES_SCHED_MODE_COMPUTE_UNIT_DEBUG:
									modeStr = "debug";
									break;
								default:
									modeStr = "unknown";
									break;
								}
								table.addRow("", "", std::format(" Scheduler Mode: {}", modeStr));

								// Get mode-specific properties
								if (mode == ZES_SCHED_MODE_TIMEOUT) {
									zes_sched_timeout_properties_t timeoutProps = {};
									if (sched->getTimeoutModeProperties(schedHandles[i], false, &timeoutProps) ==
										ZE_RESULT_SUCCESS) {
										table.addRow("", "",
													 std::format("  Timeout (us): {}", timeoutProps.watchdogTimeout));
									} else {
										table.addRow("", "", "  Timeout (us): N/A");
									}
								} else if (mode == ZES_SCHED_MODE_TIMESLICE) {
									zes_sched_timeslice_properties_t timesliceProps = {};
									if (sched->getTimesliceProperties(schedHandles[i], false, &timesliceProps) ==
										ZE_RESULT_SUCCESS) {
										table.addRow("", "", "  Timeout (us): N/A");
										table.addRow("", "",
													 std::format("  Interval (us): {}", timesliceProps.interval));
										table.addRow(
											"", "",
											std::format("  Yield Timeout (us): {}", timesliceProps.yieldTimeout));
									} else {
										table.addRow("", "", "  Timeout (us): N/A");
										table.addRow("", "", "  Interval (us): N/A");
										table.addRow("", "", "  Yield Timeout (us): N/A");
									}
								} else {
									table.addRow("", "", "  Timeout (us): N/A");
								}
							}
							break;
						}
					}
				}
			}
		}

		table.addRow("", "", "");

		if (tileId < tileCount - 1) {
			table.addSeparator();
		}
	}

	PRINT("{}", table.toString().c_str());
}

ze_result_t cmdConfig::setFrequencyRange(devInfo *d)
{
	TRACING();

	int32_t tileId = -1;
	if (configCmds[configCmdType::TILE].enabled) {
		uint32_t parsedTileId = 0;
		if (!parseUint32NoThrow(configCmds[configCmdType::TILE].val, parsedTileId)) {
			ERR("Error: Invalid tile ID.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		tileId = static_cast<int32_t>(parsedTileId);
	}

	std::string rangeStr = configCmds[configCmdType::FREQUENCYRANGE].val;
	size_t commaPos = rangeStr.find(',');

	if (commaPos == std::string::npos) {
		ERR("Invalid frequency range format. Expected format: minFrequency,maxFrequency\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	std::string minFreqStr = rangeStr.substr(0, commaPos);
	std::string maxFreqStr = rangeStr.substr(commaPos + 1);

	// Validate numeric values
	char *endPtr = nullptr;
	double minFreq = std::strtod(minFreqStr.c_str(), &endPtr);
	if (endPtr == minFreqStr.c_str() || *endPtr != '\0') {
		ERR("Invalid minimum frequency value.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	endPtr = nullptr;
	double maxFreq = std::strtod(maxFreqStr.c_str(), &endPtr);
	if (endPtr == maxFreqStr.c_str() || *endPtr != '\0') {
		ERR("Invalid maximum frequency value.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (minFreq < 0 || maxFreq < 0 || minFreq >= maxFreq) {
		ERR("Invalid frequency range values. Min frequency must be less than max frequency"
			" and both must be non-negative.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	frequency *fq = d->dev->getFrequency();
	if (fq == nullptr) {
		ERR("Error: Frequency pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	ze_result_t result = fq->setFrequencyRange(minFreq, maxFreq, tileId);

	if (result == ZE_RESULT_SUCCESS) {
		if (tileId < 0) {
			PRINT("Succeeded in changing the core frequency range on GPU {} (all tiles) to {:.0f}-{:.0f} MHz.\n",
				  d->index, minFreq, maxFreq);
		} else {
			PRINT("Succeeded in changing the core frequency range on GPU {} tile {} to {:.0f}-{:.0f} MHz.\n", d->index,
				  tileId, minFreq, maxFreq);
		}
	}

	return result;
}

/**
 * @brief Sets the power limit for the device.
 *
 * @param d Device information structure.
 *
 * @return ze_result_t Result of the operation.
 */
ze_result_t cmdConfig::setPowerLimit(devInfo *d)
{
	TRACING();

	// Parse the power limit from the option string - may include type
	std::string powerLimitStr = configCmds[configCmdType::POWERLIMIT].val;
	double powerLimit = 0.0;
	try {
		powerLimit = std::stod(powerLimitStr);
	} catch (const std::exception &) {
		// std::stod throws on non-numeric / empty input; reject it cleanly
		// instead of letting the exception abort the process.
		ERR("Invalid power limit value. Power limit must be a non-negative number.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (powerLimit < 0) {
		ERR("Invalid power limit value. Power limit must be non-negative.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	power *pwr = d->dev->getPower();
	if (pwr == nullptr) {
		ERR("Error: Power pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	uint32_t limitMw = wToMw(powerLimit);
	ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;

	powerExp *pwrExp = d->dev->getPowerExp();
	if (pwrExp != nullptr && pwrExp->isPowerExpEnabled()) {
		result = pwrExp->setPowerLimit(limitMw);
		if (result == ZE_RESULT_SUCCESS) {
			PRINT("Succeeded in setting the power limit to {:.1f} W on GPU {}\n", powerLimit, d->index);
		}
		return result;
	}

	// Determine power type if specified
	std::string powerType = configCmds[configCmdType::POWERTYPE].val;
	if (powerType.empty() || powerType == "sustain") {
		// Default to sustained power limit
		result = pwr->setLimitsExt(-1, ZES_POWER_LEVEL_SUSTAINED, limitMw);
	} else if (powerType == "burst") {
		result = pwr->setLimitsExt(-1, ZES_POWER_LEVEL_BURST, limitMw);
	} else if (powerType == "peak") {
		result = pwr->setLimitsExt(-1, ZES_POWER_LEVEL_PEAK, limitMw);
	} else {
		ERR("Invalid power type. Valid options: sustain, burst, peak\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (result == ZE_RESULT_SUCCESS) {
		PRINT("Succeeded in setting the {} power limit to {:.1f} W on GPU {}\n",
			  powerType.empty() ? "sustain" : powerType.c_str(), powerLimit, d->index);
	}

	return result;
}

/**
 * @brief Sets the standby mode for the device.
 *
 * @param d Device information structure.
 *
 * @return ze_result_t Result of the operation.
 */
ze_result_t cmdConfig::setStandby(devInfo *d)
{
	TRACING();

	// Set standby mode. Valid options are default or never
	std::string standbyMode = configCmds[configCmdType::STANDBYMODE].val;
	if (standbyMode != "default" && standbyMode != "never") {
		ERR("Invalid standby mode. Valid options are default and never.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Set the standby mode using the device class
	standby *stby = d->dev->getStandby();
	if (stby == nullptr) {
		ERR("Error: Standby pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	ze_result_t result =
		stby->setMode(standbyMode == "default" ? ZES_STANDBY_PROMO_MODE_DEFAULT : ZES_STANDBY_PROMO_MODE_NEVER);

	if (result == ZE_RESULT_SUCCESS) {
		if (configCmds[configCmdType::TILE].enabled) {
			PRINT("Succeeded in changing the standby mode on GPU {} tile {} to {}.\n", d->index,
				  configCmds[configCmdType::TILE].val.c_str(), standbyMode.c_str());
		} else {
			PRINT("Succeeded in changing the standby mode on GPU {} to {}.\n", d->index, standbyMode.c_str());
		}
	}

	return result;
}

/**
 * @brief Sets the scheduler mode for the device.
 *
 * @param d Device information structure.
 *
 * @return ze_result_t Result of the operation.
 */
ze_result_t cmdConfig::setScheduler(devInfo *d)
{
	TRACING();

	// Determine target tile(s): specific tile if -t given, all tiles otherwise
	bool singleTile = configCmds[configCmdType::TILE].enabled;
	uint32_t specificTileId = 0;
	if (singleTile) {
		if (!parseUint32NoThrow(configCmds[configCmdType::TILE].val, specificTileId)) {
			ERR("Error: Invalid tile ID.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	std::string schedulerMode = configCmds[configCmdType::SCHEDULERMODE].val;
	std::vector<std::string> parts = split(schedulerMode, ',');

	if (parts.empty()) {
		ERR("Invalid scheduler mode format.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	std::string mode = parts[0];
	std::transform(mode.begin(), mode.end(), mode.begin(),
				   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	// Validate mode and parameters before applying
	uint64_t timeoutValue = 0;
	uint64_t interval = 0;
	uint64_t yieldTimeout = 0;

	if (mode == "timeout") {
		if (parts.size() != 2) {
			ERR("Invalid timeout format. Expected: timeout,value\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		const std::string &timeoutStr = parts[1];
		if (timeoutStr.empty() || !std::all_of(timeoutStr.begin(), timeoutStr.end(), ::isdigit)) {
			ERR("Error parsing timeout value: invalid numeric format\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		timeoutValue = std::stoull(timeoutStr);
		if (timeoutValue < SCHEDULER_TIME_MIN || timeoutValue > SCHEDULER_TIME_MAX) {
			ERR("Timeout value must be between {} and {} us\n", SCHEDULER_TIME_MIN, SCHEDULER_TIME_MAX);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	} else if (mode == "timeslice") {
		if (parts.size() != 3) {
			ERR("Invalid timeslice format. Expected: timeslice,interval,yieldTimeout\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		const std::string &intervalStr = parts[1];
		const std::string &yieldStr = parts[2];
		if (intervalStr.empty() || !std::all_of(intervalStr.begin(), intervalStr.end(), ::isdigit)) {
			ERR("Error parsing timeslice values: invalid interval format\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		if (yieldStr.empty() || !std::all_of(yieldStr.begin(), yieldStr.end(), ::isdigit)) {
			ERR("Error parsing timeslice values: invalid yield timeout format\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		interval = std::stoull(intervalStr);
		yieldTimeout = std::stoull(yieldStr);
		if (interval < SCHEDULER_TIME_MIN || interval > SCHEDULER_TIME_MAX || yieldTimeout < SCHEDULER_TIME_MIN ||
			yieldTimeout > SCHEDULER_TIME_MAX) {
			ERR("Time values must be between {} and {} us\n", SCHEDULER_TIME_MIN, SCHEDULER_TIME_MAX);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	} else if (mode == "exclusive") {
		if (parts.size() != 1) {
			ERR("Invalid exclusive format. Expected: exclusive\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	} else {
		ERR("Invalid scheduler mode. Valid options: timeout, timeslice, exclusive\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;

	if (!singleTile) {
		// All tiles: use scheduler HAL which iterates all scheduler controllers in one pass
		scheduler *sched = d->dev->getScheduler();
		if (sched == nullptr) {
			ERR("Error: Scheduler pointer not found.\n");
			return ZE_RESULT_ERROR_UNKNOWN;
		}
		if (mode == "timeout") {
			result = sched->setTimeoutMode(timeoutValue);
		} else if (mode == "timeslice") {
			result = sched->setTimesliceMode(interval, yieldTimeout);
		} else {
			result = sched->setExclusiveMode();
		}
		if (result == ZE_RESULT_SUCCESS) {
			PRINT("Succeeded in changing the scheduler mode on GPU {} (all tiles) to {}\n", d->index, mode.c_str());
		}
	} else {
		// Single tile: use frequency HAL which targets a specific subdevice's scheduler
		frequency *fq = d->dev->getFrequency();
		if (fq == nullptr) {
			ERR("Error: Frequency pointer not found.\n");
			return ZE_RESULT_ERROR_UNKNOWN;
		}
		if (mode == "timeout") {
			result = fq->setSchedulerTimeoutMode(specificTileId, timeoutValue);
		} else if (mode == "timeslice") {
			result = fq->setSchedulerTimesliceMode(specificTileId, interval, yieldTimeout);
		} else {
			result = fq->setSchedulerExclusiveMode(specificTileId);
		}
		if (result == ZE_RESULT_SUCCESS) {
			PRINT("Succeeded in changing the scheduler mode on GPU {} tile {} to {}\n", d->index, specificTileId,
				  mode.c_str());
		}
	}

	return result;
}

/**
 * @brief Sets the memory ECC configuration for the device.
 *
 * @param d Device information structure.
 *
 * @return ze_result_t Result of the operation.
 */
ze_result_t cmdConfig::setMemoryEcc(devInfo *d)
{
	TRACING();
	// Set memory ECC. Valid options are 0:disable; 1:enable
	int memoryEcc = 0;
	try {
		size_t pos = 0;
		memoryEcc = stoi(configCmds[configCmdType::MEMORYECC].val, &pos);
		// Reject trailing garbage (e.g. "0,1") that stoi would otherwise ignore.
		if (pos != configCmds[configCmdType::MEMORYECC].val.size()) {
			throw std::invalid_argument("trailing characters");
		}
	} catch (const std::exception &) {
		ERR("Invalid memory ECC value. Valid options are 0:disable; 1:enable\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	if (memoryEcc != 0 && memoryEcc != 1) {
		ERR("Invalid memory ECC value. Valid options are 0:disable; 1:enable\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	// Set the memory ECC using the device class
	ecc *e = d->dev->getECC();
	if (e == nullptr) {
		ERR("Error: ECC pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	ecc_state_t state = {};
	ze_result_t result = e->setState(d->zesDeviceHdl, memoryEcc, &state);
	if (result == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
		// ECC is not supported/configurable on this device.
		if (memoryEcc == 0) {
			// Disabling ECC on hardware without ECC support is effectively a
			// no-op: the requested state (disabled) already holds, so report
			// success instead of surfacing a confusing Level Zero error.
			PRINT("Memory ECC is already disabled on GPU {} (ECC not supported).\n", d->index);
			return ZE_RESULT_SUCCESS;
		}
		// Enabling ECC on hardware that does not support it cannot succeed;
		// fail gracefully with a clear message and a non-zero exit code.
		ERR("Memory ECC configuration is not supported on GPU {}.\n", d->index);
		return result;
	}
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to set memory ECC state: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	zes_device_ecc_state_t requestedState =
		(memoryEcc == 1) ? ZES_DEVICE_ECC_STATE_ENABLED : ZES_DEVICE_ECC_STATE_DISABLED;

	if (state.currentState == requestedState) {
		PRINT("Memory ECC is already {} on GPU {} \n", (memoryEcc == 1 ? "enabled" : "disabled"), d->index);
	} else if (state.pendingState == requestedState) {
		PRINT("Successfully {} ECC memory setting on GPU {} \n", (memoryEcc == 1 ? "enabled" : "disabled"), d->index);
		PRINT("Please perform {} for the change to take effect.\n",
			  e->printEccPendingAction(state.pendingAction).c_str());
	} else {
		ERR("Failed to {} ECC memory setting on GPU {}\n", (memoryEcc == 1 ? "enable" : "disable"), d->index);
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Converts a PCIe downgrade pending action enumeration to a human-readable string.
 *
 * This function translates the zes_device_action_t enumeration value into a descriptive
 * string that indicates what action is required for PCIe downgrade changes to take effect.
 *
 * @param[in] action The device action enumeration value to convert.
 * @return std::string A human-readable string describing the required action:
 *         - "none" for ZES_DEVICE_ACTION_NONE (no action required)
 *         - "warm card reset" for ZES_DEVICE_ACTION_WARM_CARD_RESET
 *         - "cold card reset" for ZES_DEVICE_ACTION_COLD_CARD_RESET
 *         - "cold system reboot" for ZES_DEVICE_ACTION_COLD_SYSTEM_REBOOT
 *         - "none" for any unrecognized action value
 */
std::string pciDownGradePendingActionToString(zes_device_action_t action)
{
	switch (action) {
	case ZES_DEVICE_ACTION_NONE:
		return "none";
	case ZES_DEVICE_ACTION_WARM_CARD_RESET:
		return "warm card reset";
	case ZES_DEVICE_ACTION_COLD_CARD_RESET:
		return "cold card reset";
	case ZES_DEVICE_ACTION_COLD_SYSTEM_REBOOT:
		return "cold system reboot";
	default:
		break;
	}

	return "none";
}

/**
 * @brief Sets the PCIe generation downgrade configuration for the device.
 *
 * This function enables or disables PCIe link speed downgrade on the specified device.
 * It first checks if the feature is supported and verifies the current state before
 * attempting to change it. The operation may require a device reset or system reboot
 * to take effect, which will be indicated in the output message.
 *
 * @param[in] d Device information structure containing device handle and properties.
 *
 * @return ze_result_t Result of the operation.
 */
ze_result_t cmdConfig::setPCIeGenUpdate(devInfo *d)
{
	TRACING();
	// Set PCIe downgrade. Valid options are 0:disable; 1:enable
	int pcieDowngrade = 0;
	try {
		pcieDowngrade = stoi(configCmds[configCmdType::PCIEDOWNGRADE].val);
	} catch (const std::exception &) {
		// stoi throws on non-numeric / empty input; reject it cleanly instead
		// of letting the exception abort the process.
		ERR("Invalid PCIe downgrade value. Valid options are 0:disable; 1:enable\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	if (pcieDowngrade != 0 && pcieDowngrade != 1) {
		ERR("Invalid PCIe downgrade value. Valid options are 0:disable; 1:enable\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Check the PCIe downgrade status using the device class
	bool enabled = (pcieDowngrade == 1);
	PciDowngradeState state = {};
	ze_result_t result = d->dev->getPCI()->getPciDowngradeState(d->zesDeviceHdl, state);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to set PCIe downgrade state: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}
	if (!state.downgradeSupported) {
		PRINT("PCIe downgrade feature is not supported.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	if (state.downgradeEnabled && enabled) {
		PRINT("PCIe downgrade is already enabled.\n");
		return ZE_RESULT_SUCCESS;
	} else if (!state.downgradeEnabled && !enabled) {
		PRINT("PCIe downgrade is already disabled.\n");
		return ZE_RESULT_SUCCESS;
	}

	// Set the PCIe downgrade using the device class
	state = {};
	result = d->dev->getPCI()->setPciDowngradeState(d->zesDeviceHdl, enabled, state);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to set PCIe downgrade state: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	PRINT("PCIe downgrade {} successfully. please complete {} for change to take effect\n",
		  (enabled ? "enabled" : "disabled"), pciDownGradePendingActionToString(state.pendingAction).c_str());

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Resets the device.
 *
 * @param d Device information structure.
 *
 * @return ze_result_t Result of the operation.
 */
ze_result_t cmdConfig::resetDevice(devInfo *d)
{
	TRACING();

	PRINT("It may take one minute to reset GPU {}. Please wait ...\n", d->index);

	ze_result_t result = d->dev->resetDevice(d->zesDeviceHdl);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to reset device: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	PRINT("Succeed to reset the GPU {}\n", d->index);
	std::_Exit(0);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Clears all RAS error counters for the device.
 *
 * @param [in] d Device information structure.
 *
 * @return ze_result_t Result of the operation.
 */
ze_result_t cmdConfig::clearRasErrors(devInfo *d)
{
	TRACING();

	ras *rasHandler = d->dev->getRAS();

	ze_result_t result = rasHandler->clearErrors();
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to clear RAS error counters on GPU {}: 0x{:X} ({})\n", d->index, result,
			l0_error_to_string(result));
		return result;
	}

	PRINT("Successfully cleared RAS error counters on GPU {}\n", d->index);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Sets the fan speed for the device.
 *
 * Supports percentage mode:
 * - "50%" or "50" - sets fixed speed to 50 percent
 * - "default" - resets to automatic/default mode
 *
 * @param d Device information structure.
 *
 * @return ze_result_t Result of the operation.
 */
ze_result_t cmdConfig::setFanSpeed(devInfo *d)
{
	TRACING();
	int32_t fanId = -1;
	ze_result_t parseFanResult = getSelectedFanId(fanId);
	if (parseFanResult != ZE_RESULT_SUCCESS) {
		return parseFanResult;
	}
	std::string fanTarget = (fanId == -1) ? "ALL" : std::to_string(fanId);

	auto *fanHandler = reinterpret_cast<::fan *>(d->dev->getFan());
	if (fanHandler == nullptr) {
		ERR("Error: Device GPU {} does not have fan support.\n", d->index);
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	std::string fanSpeedStr = configCmds[configCmdType::FANSPEED].val;
	std::transform(fanSpeedStr.begin(), fanSpeedStr.end(), fanSpeedStr.begin(),
				   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	ze_result_t result = ZE_RESULT_SUCCESS;

	// Handle "default" mode - reset to automatic
	if (fanSpeedStr == "default") {
		result = fanHandler->setDefaultMode(fanId);
		if (result == ZE_RESULT_SUCCESS) {
			PRINT("Succeeded in setting fan {} to default (automatic) mode on GPU {}\n", fanTarget.c_str(), d->index);
		} else {
			ERR("Failed to set fan {} to default mode on GPU {}: 0x{:X} ({})\n", fanTarget.c_str(), d->index, result,
				l0_error_to_string(result));
		}
		return result;
	}

	// Parse fan speed value - can be:
	// - "50%" -> percentage
	// - "50" -> percentage
	std::string numericStr = fanSpeedStr;

	// Check for percentage suffix
	if (fanSpeedStr.length() > 0 && fanSpeedStr.back() == '%') {
		numericStr = fanSpeedStr.substr(0, fanSpeedStr.length() - 1);
	}

	// Validate that remaining string is numeric and parseable
	uint32_t speedRaw = 0;
	if (numericStr.empty() || !std::all_of(numericStr.begin(), numericStr.end(), ::isdigit) ||
		!parseUint32NoThrow(numericStr, speedRaw)) {
		ERR("Error: Invalid fan speed format: '{}'. Valid formats are:\n", fanSpeedStr.c_str());
		ERR("  - NN to set percentage (0-100)\n");
		ERR("  - NN%% to set percentage (0-100)\n");
		ERR("  - default to reset to automatic mode\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	int32_t speedValue = static_cast<int32_t>(speedRaw);
	if (speedValue < 0 || speedValue > 100) {
		ERR("Error: Fan speed percentage must be between 0 and 100, got: {}\n", speedValue);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	result = fanHandler->setFixedSpeedPercent(speedValue, fanId);
	if (result == ZE_RESULT_SUCCESS) {
		PRINT("Succeeded in setting fan {} speed to {}%% on GPU {}\n", fanTarget.c_str(), speedValue, d->index);
	} else {
		ERR("Failed to set fan {} speed to {}%% on GPU {}: 0x{:X} ({})\n", fanTarget.c_str(), speedValue, d->index,
			result, l0_error_to_string(result));
	}

	return result;
}

/**
 * @brief Sets the fan curve in percent for the device.
 *
 * Expects one or more "temp:speed" points where speed is percent (0..100),
 * optionally with '%' suffix, and temperatures are strictly increasing.
 *
 * @param d Device information structure.
 *
 * @return ze_result_t Result of the operation.
 */
ze_result_t cmdConfig::setFanCurve(devInfo *d)
{
	TRACING();
	int32_t fanId = -1;
	ze_result_t parseFanResult = getSelectedFanId(fanId);
	if (parseFanResult != ZE_RESULT_SUCCESS) {
		return parseFanResult;
	}
	std::string fanTarget = (fanId == -1) ? "ALL" : std::to_string(fanId);

	auto *fanHandler = reinterpret_cast<::fan *>(d->dev->getFan());
	if (fanHandler == nullptr) {
		ERR("Error: Device GPU {} does not have fan support.\n", d->index);
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	std::string curve = configCmds[configCmdType::FANCURVE].val;
	if (curve.empty()) {
		ERR("Error: fancurve value cannot be empty.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	auto points = split(curve, ',');
	if (points.empty()) {
		ERR("Error: invalid fancurve format. Expected temp:speed,temp:speed,...\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	std::vector<std::pair<uint32_t, int32_t>> table;
	table.reserve(points.size());
	uint32_t previousTemp = 0;
	bool first = true;

	for (const auto &entry : points) {
		size_t colonPos = entry.find(':');
		if (colonPos == std::string::npos || colonPos == 0 || colonPos + 1 >= entry.size()) {
			ERR("Error: invalid fancurve point '{}'. Expected temp:speed\n", entry.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		std::string tempStr = entry.substr(0, colonPos);
		std::string speedStr = entry.substr(colonPos + 1);

		if (tempStr.empty() || !std::all_of(tempStr.begin(), tempStr.end(), ::isdigit)) {
			ERR("Error: invalid temperature '{}' in fancurve.\n", tempStr.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		if (!speedStr.empty() && speedStr.back() == '%') {
			speedStr = speedStr.substr(0, speedStr.size() - 1);
		} else if (speedStr.size() >= 3 && speedStr.substr(speedStr.size() - 3) == "rpm") {
			ERR("Error: --fancurve supports percent points only. Use --fancurve-rpm for RPM points.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		if (speedStr.empty() || !std::all_of(speedStr.begin(), speedStr.end(), ::isdigit)) {
			ERR("Error: invalid fan speed value in fancurve point '{}'.\n", entry.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		uint32_t temp = 0;
		if (!parseUint32NoThrow(tempStr, temp)) {
			ERR("Error: invalid numeric value in fancurve point '{}'.\n", entry.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		if (temp < FAN_CURVE_TEMP_MIN_C || temp > FAN_CURVE_TEMP_MAX_C) {
			ERR("Error: fancurve temperature must be in range {}..{} C.\n", FAN_CURVE_TEMP_MIN_C, FAN_CURVE_TEMP_MAX_C);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		uint32_t speedRaw = 0;
		if (!parseUint32NoThrow(speedStr, speedRaw)) {
			ERR("Error: invalid numeric value in fancurve point '{}'.\n", entry.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		if (speedRaw > 100) {
			ERR("Error: fancurve percent speed must be in range 0..100.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		int32_t speed = static_cast<int32_t>(speedRaw);
		if (!first && temp <= previousTemp) {
			ERR("Error: fancurve temperatures must be strictly increasing.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		table.emplace_back(temp, speed);
		previousTemp = temp;
		first = false;
	}

	ze_result_t result = fanHandler->setSpeedTableMode(table, ZES_FAN_SPEED_UNITS_PERCENT, fanId);
	if (result == ZE_RESULT_SUCCESS) {
		PRINT("Succeeded in setting fan {} curve on GPU {} with {} points (percent)\n", fanTarget.c_str(), d->index,
			  table.size());
	} else {
		ERR("Failed to set fan {} curve on GPU {}: 0x{:X} ({})\n", fanTarget.c_str(), d->index, result,
			l0_error_to_string(result));
	}

	return result;
}

/**
 * @brief Sets the fan curve in RPM for the device.
 *
 * Expects one or more "temp:rpm" points where rpm is non-negative,
 * optionally with "rpm" suffix, and temperatures are strictly increasing.
 *
 * @param d Device information structure.
 *
 * @return ze_result_t Result of the operation.
 */
ze_result_t cmdConfig::setFanCurveRpm(devInfo *d)
{
	TRACING();
	int32_t fanId = -1;
	ze_result_t parseFanResult = getSelectedFanId(fanId);
	if (parseFanResult != ZE_RESULT_SUCCESS) {
		return parseFanResult;
	}
	std::string fanTarget = (fanId == -1) ? "ALL" : std::to_string(fanId);

	auto *fanHandler = reinterpret_cast<::fan *>(d->dev->getFan());
	if (fanHandler == nullptr) {
		ERR("Error: Device GPU {} does not have fan support.\n", d->index);
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	std::string curve = configCmds[configCmdType::FANCURVERPM].val;
	if (curve.empty()) {
		ERR("Error: fancurve-rpm value cannot be empty.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	auto points = split(curve, ',');
	if (points.empty()) {
		ERR("Error: invalid fancurve-rpm format. Expected temp:rpm,temp:rpm,...\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	std::vector<std::pair<uint32_t, int32_t>> table;
	table.reserve(points.size());
	uint32_t previousTemp = 0;
	bool first = true;

	for (const auto &entry : points) {
		size_t colonPos = entry.find(':');
		if (colonPos == std::string::npos || colonPos == 0 || colonPos + 1 >= entry.size()) {
			ERR("Error: invalid fancurve-rpm point '{}'. Expected temp:rpm\n", entry.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		std::string tempStr = entry.substr(0, colonPos);
		std::string speedStr = entry.substr(colonPos + 1);

		if (tempStr.empty() || !std::all_of(tempStr.begin(), tempStr.end(), ::isdigit)) {
			ERR("Error: invalid temperature '{}' in fancurve-rpm.\n", tempStr.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		if (!speedStr.empty() && speedStr.back() == '%') {
			ERR("Error: --fancurve-rpm supports RPM points only. Use --fancurve for percent points.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		if (speedStr.size() >= 3 && speedStr.substr(speedStr.size() - 3) == "rpm") {
			speedStr = speedStr.substr(0, speedStr.size() - 3);
		}

		if (speedStr.empty() || !std::all_of(speedStr.begin(), speedStr.end(), ::isdigit)) {
			ERR("Error: invalid RPM value in fancurve-rpm point '{}'.\n", entry.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		uint32_t temp = 0;
		if (!parseUint32NoThrow(tempStr, temp)) {
			ERR("Error: invalid numeric value in fancurve-rpm point '{}'.\n", entry.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		if (temp < FAN_CURVE_TEMP_MIN_C || temp > FAN_CURVE_TEMP_MAX_C) {
			ERR("Error: fancurve-rpm temperature must be in range {}..{} C.\n", FAN_CURVE_TEMP_MIN_C,
				FAN_CURVE_TEMP_MAX_C);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		uint32_t speedRaw = 0;
		if (!parseUint32NoThrow(speedStr, speedRaw)) {
			ERR("Error: invalid numeric value in fancurve-rpm point '{}'.\n", entry.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		if (speedRaw < static_cast<uint32_t>(FAN_CURVE_RPM_MIN) ||
			speedRaw > static_cast<uint32_t>(FAN_CURVE_RPM_MAX)) {
			ERR("Error: fancurve-rpm speed must be in range {}..{} RPM.\n", FAN_CURVE_RPM_MIN, FAN_CURVE_RPM_MAX);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		int32_t speed = static_cast<int32_t>(speedRaw);
		if (!first && temp <= previousTemp) {
			ERR("Error: fancurve-rpm temperatures must be strictly increasing.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		table.emplace_back(temp, speed);
		previousTemp = temp;
		first = false;
	}

	ze_result_t result = fanHandler->setSpeedTableMode(table, ZES_FAN_SPEED_UNITS_RPM, fanId);
	if (result == ZE_RESULT_SUCCESS) {
		PRINT("Succeeded in setting fan {} curve on GPU {} with {} points (rpm)\n", fanTarget.c_str(), d->index,
			  table.size());
	} else {
		ERR("Failed to set fan {} curve on GPU {}: 0x{:X} ({})\n", fanTarget.c_str(), d->index, result,
			l0_error_to_string(result));
	}

	return result;
}

/**
 * @brief Cold resets the device via PCIe slot power cycle.
 *
 * Aborts if processes are using the GPU (override with --ignore-gpu-user-processes)
 * or if other PCI devices share the slot (override with --force-reset-gpus).
 *
 * On success the process exits via std::_Exit(0) because the GPU has
 * disappeared and re-enumerated; outstanding Level Zero handles are no
 * longer valid and clean teardown is unsafe.
 *
 * @param d Device information structure.
 *
 * @return ze_result_t Result of the operation. The function does not return
 *         on success (process exits).
 */
ze_result_t cmdConfig::coldResetDevice(devInfo *d)
{
	TRACING();

	if (!PRIVILEGECHECK()) {
		ERR("Cold reset requires elevated privileges (root or 'xpum' group on Linux).\n");
		return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
	}

	const std::string bdfStr = d->dev->getBDFStr();

	if (!configCmds[configCmdType::IGNORE_GPU_USER_PROCESSES].enabled) {
		std::vector<uint32_t> pids = getGpuProcessesByBdf(bdfStr);
		if (!pids.empty()) {
			ERR("Cold reset aborted: {} process(es) are currently using GPU {} ({}):\n", pids.size(), d->index, bdfStr);
			for (uint32_t pid : pids) {
				std::string procName = GETPROCESSNAME(pid);
				ERR("  PID {} ({})\n", pid, procName.empty() ? "<unknown>" : procName.c_str());
			}
			ERR("Terminate these processes and re-run, or pass --ignore-gpu-user-processes to\n"
				"proceed.\n");
			return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
		}
	}

	if (!configCmds[configCmdType::FORCE_RESET_GPUS].enabled) {
		std::vector<std::string> peers = getDevicesSharingSlotWith(bdfStr);
		if (!peers.empty()) {
			ERR("Cold reset aborted: {} other PCI device(s) share the PCIe slot with GPU {} ({})\n"
				"and would be reset by this operation:\n",
				peers.size(), d->index, bdfStr);
			for (const auto &peerBdf : peers) {
				ERR("  {}\n", peerBdf);
			}
			ERR("Pass --force-reset-gpus to proceed and reset all listed devices.\n");
			return ZE_RESULT_ERROR_NOT_AVAILABLE;
		}
	}

	PRINT("Performing cold reset on GPU {} ({}). Please wait ...\n", d->index, bdfStr);

	ze_result_t result = d->dev->coldResetDevice();
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to cold reset device: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	PRINT("Cold reset succeeded for GPU {}\n", d->index);
	std::fflush(stdout);
	std::_Exit(0);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the config run.
 *
 * @return int Returns 0 on success.
 */
int cmdConfig::run(arg_struct *args)
{
	TRACING();
	std::vector<devInfo> deviceList;
	ze_result_t result;
	bool isQueryMode = true;

	// Reset state
	for (auto &[k, v] : configCmds) {
		v.enabled = false;
		v.val.clear();
	}

	CLI::App sub{"Configure GPU settings", "config"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", configCmds[configCmdType::CONFIGJSON].enabled, "Print result in JSON format");
	sub.add_option("-d,--device,--id", configCmds[configCmdType::CONFIGDEVICE].val, "Device ID or PCI BDF address")
		->each([&](const std::string &) { configCmds[configCmdType::CONFIGDEVICE].enabled = true; });
	sub.add_option("-t,--tile", configCmds[configCmdType::TILE].val, "Tile ID")->each([&](const std::string &) {
		configCmds[configCmdType::TILE].enabled = true;
	});
	sub.add_option("--frequencyrange", configCmds[configCmdType::FREQUENCYRANGE].val,
				   "Set frequency range (MHz), e.g. minFreq:maxFreq")
		->each([&](const std::string &) {
			configCmds[configCmdType::FREQUENCYRANGE].enabled = true;
			isQueryMode = false;
		});
	sub.add_option("--powerlimit", configCmds[configCmdType::POWERLIMIT].val, "Set power limit (W)")
		->each([&](const std::string &) {
			configCmds[configCmdType::POWERLIMIT].enabled = true;
			isQueryMode = false;
		});
	sub.add_option("--standby", configCmds[configCmdType::STANDBYMODE].val, "Set standby mode")
		->each([&](const std::string &) {
			configCmds[configCmdType::STANDBYMODE].enabled = true;
			isQueryMode = false;
		});
	sub.add_option("--scheduler", configCmds[configCmdType::SCHEDULERMODE].val, "Set scheduler mode")
		->each([&](const std::string &) {
			configCmds[configCmdType::SCHEDULERMODE].enabled = true;
			isQueryMode = false;
		});
	sub.add_option("--memoryecc", configCmds[configCmdType::MEMORYECC].val, "Enable (1) or disable (0) ECC")
		->each([&](const std::string &) {
			configCmds[configCmdType::MEMORYECC].enabled = true;
			isQueryMode = false;
		});
	sub.add_option("--pciedowngrade", configCmds[configCmdType::PCIEDOWNGRADE].val, "Set PCIe generation downgrade")
		->each([&](const std::string &) {
			configCmds[configCmdType::PCIEDOWNGRADE].enabled = true;
			isQueryMode = false;
		});
	sub.add_flag("--reset", configCmds[configCmdType::RESET].enabled, "Reset the GPU");
	sub.add_option("--powertype", configCmds[configCmdType::POWERTYPE].val, "Power type")
		->each([&](const std::string &) { configCmds[configCmdType::POWERTYPE].enabled = true; });
	sub.add_flag("--clear-ras-errors", configCmds[configCmdType::CLEARRAS].enabled, "Clear RAS error counters");
	sub.add_option("--fanspeed", configCmds[configCmdType::FANSPEED].val,
				   "Set fan speed (%). Use --fanid to target a specific fan; omit for all fans")
		->each([&](const std::string &) {
			configCmds[configCmdType::FANSPEED].enabled = true;
			isQueryMode = false;
		});
	sub.add_option("--fancurve", configCmds[configCmdType::FANCURVE].val,
				   "Set fan curve as temp:speed pairs, e.g. 50:20,80:60,100:100")
		->each([&](const std::string &) {
			configCmds[configCmdType::FANCURVE].enabled = true;
			isQueryMode = false;
		});
	sub.add_option("--fancurve-rpm", configCmds[configCmdType::FANCURVERPM].val,
				   "Set fan curve in RPM as temp:rpm pairs, e.g. 50:800,80:1200")
		->each([&](const std::string &) {
			configCmds[configCmdType::FANCURVERPM].enabled = true;
			isQueryMode = false;
		});
	sub.add_option("--fanid", configCmds[configCmdType::FANID].val,
				   "Target fan ID (-1 = all fans, 0..N-1 = specific fan)")
		->each([&](const std::string &) { configCmds[configCmdType::FANID].enabled = true; });
	sub.add_flag("--coldreset", configCmds[configCmdType::COLDRESET].enabled,
				 "Cold-reset the device (requires PCI BDF address for --device, not a device index)");
	sub.add_flag("--ignore-gpu-user-processes", configCmds[configCmdType::IGNORE_GPU_USER_PROCESSES].enabled,
				 "Skip check for running GPU user processes before reset");
	sub.add_flag("--force-reset-gpus", configCmds[configCmdType::FORCE_RESET_GPUS].enabled,
				 "Force GPU reset even if user processes are present");

	try {
		sub.parse(args->argc - 1, args->argv + 1);
	} catch (const CLI::CallForHelp &) {
		help();
		return ZE_RESULT_SUCCESS;
	} catch (const CLI::ParseError &e) {
		ERR("{}", e.what());
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Flags that constitute a write (non-query) operation
	if (configCmds[configCmdType::RESET].enabled || configCmds[configCmdType::CLEARRAS].enabled ||
		configCmds[configCmdType::COLDRESET].enabled) {
		isQueryMode = false;
	}

	// Check if the device ID is provided
	if (configCmds[configCmdType::CONFIGDEVICE].val.empty()) {
		ERR("Device ID is required.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (configCmds[configCmdType::COLDRESET].enabled) {
		const std::string &devArg = configCmds[configCmdType::CONFIGDEVICE].val;
		if (!isValidBdf(devArg)) {
			ERR("Cold reset requires a PCI BDF address in DDDD:BB:DD.F format "
				"(e.g. 0000:4d:00.0), not a device ID.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	result = args->sm.findDevice(configCmds[configCmdType::CONFIGDEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '{}'.\n",
			configCmds[configCmdType::CONFIGDEVICE].val.c_str());
		return result;
	}

	if (configCmds[configCmdType::RESET].enabled) {
		if (configCmds[configCmdType::TILE].enabled) {
			ERR("Reset does not support tile ID.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		if (deviceList.size() != 1) {
			ERR("Reset requires a single device ID.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	if (configCmds[configCmdType::COLDRESET].enabled) {
		if (configCmds[configCmdType::TILE].enabled) {
			ERR("Cold reset does not support tile ID.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		if (deviceList.size() != 1) {
			ERR("Cold reset requires a single PCI BDF.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	// If this is query mode, display configuration
	if (isQueryMode) {
		if (configCmds[configCmdType::CONFIGJSON].enabled) {
			if (deviceList.size() == 1) {
				std::string jsonOut = buildDeviceConfigJson(&deviceList[0], 0);
				PRINT("{}\n", jsonOut.c_str());
			} else {
				std::ostringstream out;
				out << "[\n";
				for (size_t i = 0; i < deviceList.size(); ++i) {
					out << buildDeviceConfigJson(&deviceList[i], 4);
					if (i + 1 < deviceList.size()) {
						out << ",";
					}
					out << "\n";
				}
				out << "]\n";
				PRINT("{}", out.str().c_str());
			}
		} else {
			for (auto &device : deviceList) {
				displayDeviceConfig(&device);
			}
		}
		return ZE_RESULT_SUCCESS;
	}

	// Otherwise, execute set commands
	ze_result_t firstError = ZE_RESULT_SUCCESS;
	bool hadError = false;

	for (auto &device : deviceList) {
		for (const auto &cmd : configCmds) {
			if (cmd.second.enabled && cmd.second.func != nullptr) {
				result = (this->*cmd.second.func)(&device);
				if (result != ZE_RESULT_SUCCESS) {
					if (!hadError) {
						firstError = result;
						hadError = true;
					}
					// Continue processing other devices/commands instead of early return
					ERR("Failed to apply configuration to GPU {}, continuing with remaining devices...\n",
						device.index);
				}
			}
		}
	}

	// Return the first error encountered, or success if all succeeded
	return hadError ? firstError : ZE_RESULT_SUCCESS;
}
