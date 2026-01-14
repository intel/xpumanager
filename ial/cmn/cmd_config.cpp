/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_config.h"
#include "debug.h"
#include <assert.h>
#include <ecc.h>
#include <fabric.h>
#include <frequency.h>
#include <power.h>
#include <scheduler.h>
#include <performance.h>
#include <standby.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cinttypes>

// Conversion factors
constexpr uint64_t W_TO_MW = 1000;

// Scheduler time limits in microseconds
constexpr uint64_t SCHEDULER_TIME_MIN = 5000;
constexpr uint64_t SCHEDULER_TIME_MAX = 100000000;

static std::unordered_map<configCmdType, configCmdStruct> configCmds = {
	{configCmdType::CONFIGHELP, {{"help", no_argument, 0, 'h'}, nullptr, false, ""}},
	{configCmdType::CONFIGJSON, {{"json", no_argument, 0, 'j'}, nullptr, false, ""}},
	{configCmdType::CONFIGDEVICE, {{"device", required_argument, 0, 'd'}, nullptr, false, ""}},
	{configCmdType::TILE, {{"tile", required_argument, 0, 't'}, nullptr, false, ""}},
	{configCmdType::FREQUENCYRANGE,
	 {{"frequencyrange", required_argument, 0, 0}, &cmdConfig::setFrequencyRange, false, ""}},
	{configCmdType::POWERLIMIT, {{"powerlimit", required_argument, 0, 0}, &cmdConfig::setPowerLimit, false, ""}},
	{configCmdType::STANDBYMODE, {{"standby", required_argument, 0, 0}, &cmdConfig::setStandby, false, ""}},
	{configCmdType::SCHEDULERMODE, {{"scheduler", required_argument, 0, 0}, &cmdConfig::setScheduler, false, ""}},
	{configCmdType::PERFORMANCEFACTOR,
	 {{"performancefactor", required_argument, 0, 0}, &cmdConfig::setPerformanceFactor, false, ""}},
	{configCmdType::XELINKPORT, {{"xelinkport", required_argument, 0, 0}, &cmdConfig::setXeLinkPort, false, ""}},
	{configCmdType::XELINKPORTBEACONING,
	 {{"xelinkportbeaconing", required_argument, 0, 0}, &cmdConfig::setXeLinkPortBeaconing, false, ""}},
	{configCmdType::MEMORYECC, {{"memoryecc", required_argument, 0, 0}, &cmdConfig::setMemoryEcc, false, ""}},
	{configCmdType::PCIEDOWNGRADE,
	 {{"pciedowngrade", required_argument, 0, 0}, &cmdConfig::setPCIeGenUpdate, false, ""}},
	{configCmdType::RESET, {{"reset", no_argument, 0, 0}, &cmdConfig::resetDevice, false, ""}},
	{configCmdType::PPR, {{"ppr", no_argument, 0, 0}, &cmdConfig::applyPpr, false, ""}},
	{configCmdType::FORCE, {{"force", no_argument, 0, 0}, &cmdConfig::forcePpr, false, ""}},
	{configCmdType::POWERTYPE, {{"powertype", required_argument, 0, 0}, nullptr, false, ""}},
};

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
	helpList.push_back(helpCmd(HEADING, "%s config -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(
		HEADING, "%s config -d [deviceId] -t [tileId] --frequencyrange [minFrequency,maxFrequency]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s config -d [deviceId] --powerlimit [powerValue]", progName.c_str()));
	helpList.push_back(
		helpCmd(HEADING, "%s config -d [deviceId] -t [tileId] --standby [standbyMode]", progName.c_str()));
	helpList.push_back(
		helpCmd(HEADING, "%s config -d [deviceId] -t [tileId] --scheduler [schedulerMode]", progName.c_str()));
	helpList.push_back(helpCmd(
		HEADING, "%s config -d [deviceId] -t [tileId] --performancefactor [engineType,factorValue]", progName.c_str()));
	helpList.push_back(
		helpCmd(HEADING, "%s config -d [deviceId] -t [tileId] --xelinkport [portId,value]", progName.c_str()));
	helpList.push_back(
		helpCmd(HEADING, "%s config -d [deviceId] -t [tileId] --xelinkportbeaconing [portId,value]", progName.c_str()));
	helpList.push_back(
		helpCmd(HEADING, "%s config -d [deviceId] --memoryecc [0|1] 0:disable; 1:enable", progName.c_str()));
	helpList.push_back(
		helpCmd(HEADING, "%s config -d [deviceId] --pciedowngrade [0|1] 0:disable; 1:enable", progName.c_str()));

	helpList.push_back(helpCmd(HEADING, "%s config -d [deviceId] --reset", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s config -d [deviceId] --ppr", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 The device ID or PCI BDF address to query"));
	helpList.push_back(helpCmd(HEADING, "-t,--tile                   The tile ID"));
	helpList.push_back(helpCmd(HEADING, "--frequencyrange            GPU tile-level core frequency range"));
	helpList.push_back(helpCmd(HEADING, "--powerlimit                Device-level power limit"));
	helpList.push_back(
		helpCmd(HEADING, "--standby                   Tile-level standby mode. Valid options: \"default\"; \"never\""));
	helpList.push_back(
		helpCmd(HEADING, "--scheduler                 Tile-level scheduler mode. Value options: "
						 "\"timeout\",timeoutValue (us); \"timeslice\",interval (us),yieldtimeout (us);\"exclusive\""));
	helpList.push_back(helpCmd(SUB_HEADING, "The valid range of all time values (us) is from 5000 to 100,000,000."));
	helpList.push_back(helpCmd(HEADING, "--reset                     Reset device by SBR (Secondary Bus Reset)"));
	helpList.push_back(helpCmd(SUB_HEADING, "For Intel(R) Max Series GPU, when SR-IOV is enabled, please add "
											"\"pci=realloc=off\" into Linux kernel command line parameters"));
	helpList.push_back(
		helpCmd(SUB_HEADING,
				"When SR-IOV is disabled, please add \"pci=realloc=on\" into Linux kernel command line parameters"));
	helpList.push_back(helpCmd(HEADING, "--ppr                       Apply ppr to the device"));
	helpList.push_back(helpCmd(HEADING, "--force                     Force PPR to run"));
	helpList.push_back(helpCmd(HEADING, "--performancefactor         Set the tile-level performance factor. Valid "
										"options: \"compute/media\",factorValue. The factor value should be"));
	helpList.push_back(helpCmd(SUB_HEADING, "between 0 to 100. 100 means that the workload is completely compute "
											"bounded and requires very little support from the memory support"));
	helpList.push_back(helpCmd(SUB_HEADING, "0 means that the workload is completely memory bounded and the "
											"performance of the memory controller needs to be increased"));
	helpList.push_back(helpCmd(
		HEADING, "--xelinkport                Change the Xe Link port status. The value 0 means down and 1 means up"));
	helpList.push_back(helpCmd(
		HEADING,
		"--xelinkportbeaconing       Change the Xe Link port beaconing status. The value 0 means off and 1 means on"));
	helpList.push_back(
		helpCmd(HEADING, "--memoryecc                 Enable/disable memory ECC setting. 0:disable; 1:enable"));
	helpList.push_back(helpCmd(HEADING, "--powerlimit                Device-level power limit"));
	helpList.push_back(helpCmd(
		HEADING,
		"--powertype                 Device-level power limit type. Valid options: \"sustain\"; \"peak\"; \"burst\""));
	helpList.push_back(
		helpCmd(HEADING, "--pciedowngrade                 Enable/disable PCIe downgrade setting. 0:disable; 1:enable"));

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Displays the current device configuration including power, ECC, and tile-level settings.
 *
 * This function queries and displays comprehensive device configuration information including:
 * - Power limits (sustained, burst, peak, instantaneous)
 * - Memory ECC state (current and pending)
 * - Tile-level configurations (frequency, standby, scheduler, performance)
 * - Xe Link port status and beaconing configuration
 *
 * @param[in] d Device information structure containing device handle and properties.
 */
void cmdConfig::displayDeviceConfig(devInfo *d)
{
	TRACING();

	PRINT("+--------------+-------------------+----------------------------------------------------------------+\n");
	PRINT("| Device Type  | Device ID/Tile ID | Configuration                                                  |\n");
	PRINT("+--------------+-------------------+----------------------------------------------------------------+\n");

	// Power configuration - using getLimitsExt()
	power *pwr = d->dev->getPower();
	if (pwr != nullptr) {
		std::vector<PowerLimitExt> limits;
		if (pwr->getLimitsExt(limits) == ZE_RESULT_SUCCESS) {
			for (const auto &limit : limits) {
				const char *levelStr = "Unknown";
				switch (limit.level) {
				case ZES_POWER_LEVEL_SUSTAINED:
					levelStr = "Sustained";
					break;
				case ZES_POWER_LEVEL_BURST:
					levelStr = "Burst";
					break;
				case ZES_POWER_LEVEL_PEAK:
					levelStr = "Peak";
					break;
				case ZES_POWER_LEVEL_INSTANTANEOUS:
					levelStr = "Instantaneous";
					break;
				case ZES_POWER_LEVEL_UNKNOWN:
				case ZES_POWER_LEVEL_FORCE_UINT32:
					levelStr = "Unknown";
					break;
				}
				PRINT("| GPU          | %-17d | Power Limit - %s (W): %.1f%s                      |\n", d->index,
					  levelStr, static_cast<float>(limit.limitMw) / 1000.0f, limit.locked ? " [LOCKED]" : "");
			}
		}
	}

	// ECC configuration
	ecc *e = d->dev->getECC();
	if (e != nullptr) {
		zes_device_ecc_properties_t state = {};
		if (e->getState(d->zesDeviceHdl, &state) == ZE_RESULT_SUCCESS) {
			PRINT("|              |                   | Memory ECC:                                                    "
				  "|\n");
			PRINT("|              |                   |   Current: %-54s |\n",
				  state.currentState == ZES_DEVICE_ECC_STATE_ENABLED ? "enabled" : "disabled");
			PRINT("|              |                   |   Pending: %-54s |\n",
				  state.pendingState == ZES_DEVICE_ECC_STATE_ENABLED ? "enabled" : "disabled");
		}
	}

	PRINT("+--------------+-------------------+----------------------------------------------------------------+\n");

	// Display tile-level configuration - NOTE: frequency class doesn't expose tile iteration
	// We'll need to query fabric ports to get tile info or use device properties
	uint32_t tileCount = 0;
	zes_device_properties_t props = {};
	props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
	if (zesDeviceGetProperties(d->zesDeviceHdl, &props) == ZE_RESULT_SUCCESS) {
		tileCount = props.numSubdevices;
	}

	// If no subdevices, show device-level config
	if (tileCount == 0) {
		tileCount = 1;
	}

	for (uint32_t tileId = 0; tileId < tileCount; tileId++) {
		std::stringstream tileIdStr;
		tileIdStr << d->index << "/" << tileId;

		PRINT("| GPU          | %-17s |", tileIdStr.str().c_str());

		// Frequency - we cannot query per-tile easily with current HAL
		// The frequency class only has device-level getRange(), not per-tile
		// This is a HAL limitation - would need enhancement
		frequency *fq = d->dev->getFrequency();
		if (fq != nullptr && tileId == 0) { // Only show for first tile as HAL is device-level
			PRINT(" GPU Frequency: See device properties                           |\n");
		} else {
			PRINT("                                                                |\n");
		}

		// Standby configuration
		// Note: standby::getMode requires a standby handle as parameter
		// Since we cannot access private members, we skip displaying standby mode
		// The setMode function still works as it handles enumeration internally

		// The scheduler class doesn't expose per-tile querying easily
		// Would need to enumerate schedulers and match by subdeviceId
		PRINT("|              |                   | Scheduler: [Use 'xpu-smi discovery' for details]              |\n");

		// The performance class doesn't expose easy per-tile querying
		PRINT("|              |                   | Performance: [Use 'xpu-smi discovery' for details]            |\n");

		// Fabric/XeLink configuration
		fabric *f = d->dev->getFabric();
		if (f != nullptr) {
			std::vector<portInfo> portInfoList;
			if (f->getFabricPorts(d->zesDeviceHdl, portInfoList) == ZE_RESULT_SUCCESS && !portInfoList.empty()) {
				PRINT("|              |                   | Xe Link ports:                                             "
					  "    "
					  "|\n");

				std::vector<uint32_t> upPorts, downPorts, beaconingOnPorts, beaconingOffPorts;

				for (const auto &pi : portInfoList) {
					if (pi.portProps.onSubdevice && pi.portProps.subdeviceId != tileId) {
						continue; // Skip ports not on this tile
					}

					uint32_t portNum = pi.portProps.portId.portNumber;

					if (pi.portState.status == ZES_FABRIC_PORT_STATUS_HEALTHY) {
						upPorts.push_back(portNum);
					} else {
						downPorts.push_back(portNum);
					}

					if (pi.portConf.beaconing) {
						beaconingOnPorts.push_back(portNum);
					} else {
						beaconingOffPorts.push_back(portNum);
					}
				}

				// Display port statuses
				if (!upPorts.empty()) {
					std::stringstream ss;
					for (size_t i = 0; i < upPorts.size(); i++) {
						ss << upPorts[i];
						if (i < upPorts.size() - 1)
							ss << ", ";
					}
					PRINT("|              |                   |   Up: %-58s |\n", ss.str().c_str());
				}
				if (!downPorts.empty()) {
					std::stringstream ss;
					for (size_t i = 0; i < downPorts.size(); i++) {
						ss << downPorts[i];
						if (i < downPorts.size() - 1)
							ss << ", ";
					}
					PRINT("|              |                   |   Down: %-56s |\n", ss.str().c_str());
				}
				if (!beaconingOnPorts.empty()) {
					std::stringstream ss;
					for (size_t i = 0; i < beaconingOnPorts.size(); i++) {
						ss << beaconingOnPorts[i];
						if (i < beaconingOnPorts.size() - 1)
							ss << ", ";
					}
					PRINT("|              |                   |   Beaconing On: %-48s |\n", ss.str().c_str());
				}
				if (!beaconingOffPorts.empty()) {
					std::stringstream ss;
					for (size_t i = 0; i < beaconingOffPorts.size(); i++) {
						ss << beaconingOffPorts[i];
						if (i < beaconingOffPorts.size() - 1)
							ss << ", ";
					}
					PRINT("|              |                   |   Beaconing Off: %-47s |\n", ss.str().c_str());
				}
			}
		}

		if (tileId < tileCount - 1) {
			PRINT("+--------------+-------------------+----------------------------------------------------------------"
				  "+\n");
		}
	}

	PRINT("+--------------+-------------------+----------------------------------------------------------------+\n");
}

/**
 * @brief Sets the frequency range for the device.
 *
 * @param d Device information structure.
 *
 * @return ze_result_t Result of the operation.
 */
ze_result_t cmdConfig::setFrequencyRange(devInfo *d)
{
	TRACING();

	if (!configCmds[configCmdType::TILE].enabled) {
		ERR("Error: Tile ID is required for frequency configuration. Use -t <tileId>.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Validate tile ID is numeric
	const std::string &tileIdStr = configCmds[configCmdType::TILE].val;
	if (tileIdStr.empty() || !std::all_of(tileIdStr.begin(), tileIdStr.end(), ::isdigit)) {
		ERR("Error: Invalid tile ID.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	int tileId = std::stoi(tileIdStr);

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
		PRINT("Succeeded in changing the core frequency range on GPU %d tile %d to %.0f-%.0f MHz.\n", d->index, tileId,
			  minFreq, maxFreq);
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
	double powerLimit = std::stod(powerLimitStr);

	if (powerLimit < 0) {
		ERR("Invalid power limit value. Power limit must be non-negative.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	power *pwr = d->dev->getPower();
	if (pwr == nullptr) {
		ERR("Error: Power pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	uint32_t limitMw = static_cast<uint32_t>(powerLimit * W_TO_MW);
	ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;

	// Determine power type if specified
	std::string powerType = configCmds[configCmdType::POWERTYPE].val;
	if (powerType.empty() || powerType == "sustain") {
		// Default to sustained power limit
		result = pwr->setSustainedLimit(limitMw, -1);
	} else if (powerType == "burst") {
		result = pwr->setBurstLimit(limitMw);
	} else if (powerType == "peak") {
		result = pwr->setPeakLimit(limitMw, limitMw);
	} else {
		ERR("Invalid power type. Valid options: sustain, burst, peak\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (result == ZE_RESULT_SUCCESS) {
		PRINT("Succeeded in setting the %s power limit to %.1f W on GPU %d\n",
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
			PRINT("Succeeded in changing the standby mode on GPU %d tile %s to %s.\n", d->index,
				  configCmds[configCmdType::TILE].val.c_str(), standbyMode.c_str());
		} else {
			PRINT("Succeeded in changing the standby mode on GPU %d to %s.\n", d->index, standbyMode.c_str());
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

	if (!configCmds[configCmdType::TILE].enabled) {
		ERR("Error: Tile ID is required for scheduler configuration. Use -t <tileId>.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Validate tile ID is numeric
	const std::string &tileIdStr = configCmds[configCmdType::TILE].val;
	if (tileIdStr.empty() || !std::all_of(tileIdStr.begin(), tileIdStr.end(), ::isdigit)) {
		ERR("Error: Invalid tile ID.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	uint32_t tileId = static_cast<uint32_t>(std::stoul(tileIdStr));

	std::string schedulerMode = configCmds[configCmdType::SCHEDULERMODE].val;
	std::vector<std::string> parts = split(schedulerMode, ',');

	if (parts.empty()) {
		ERR("Invalid scheduler mode format.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	frequency *fq = d->dev->getFrequency();
	if (fq == nullptr) {
		ERR("Error: Frequency pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	std::string mode = parts[0];
	std::transform(mode.begin(), mode.end(), mode.begin(),
				   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;

	if (mode == "timeout") {
		if (parts.size() != 2) {
			ERR("Invalid timeout format. Expected: timeout,value\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		// Validate numeric value
		const std::string &timeoutStr = parts[1];
		if (timeoutStr.empty() || !std::all_of(timeoutStr.begin(), timeoutStr.end(), ::isdigit)) {
			ERR("Error parsing timeout value: invalid numeric format\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		uint64_t timeoutValue = std::stoull(timeoutStr);
		if (timeoutValue < SCHEDULER_TIME_MIN || timeoutValue > SCHEDULER_TIME_MAX) {
			ERR("Timeout value must be between %" PRIu64 " and %" PRIu64 " us\n", SCHEDULER_TIME_MIN,
				SCHEDULER_TIME_MAX);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		result = fq->setSchedulerTimeoutMode(tileId, timeoutValue);
	} else if (mode == "timeslice") {
		if (parts.size() != 3) {
			ERR("Invalid timeslice format. Expected: timeslice,interval,yieldTimeout\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		// Validate both numeric values
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

		uint64_t interval = std::stoull(intervalStr);
		uint64_t yieldTimeout = std::stoull(yieldStr);

		if (interval < SCHEDULER_TIME_MIN || interval > SCHEDULER_TIME_MAX || yieldTimeout < SCHEDULER_TIME_MIN ||
			yieldTimeout > SCHEDULER_TIME_MAX) {
			ERR("Time values must be between %" PRIu64 " and %" PRIu64 " us\n", SCHEDULER_TIME_MIN, SCHEDULER_TIME_MAX);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		result = fq->setSchedulerTimesliceMode(tileId, interval, yieldTimeout);
	} else if (mode == "exclusive") {
		result = fq->setSchedulerExclusiveMode(tileId);
	} else {
		ERR("Invalid scheduler mode. Valid options: timeout, timeslice, exclusive\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (result == ZE_RESULT_SUCCESS) {
		PRINT("Succeeded in changing the scheduler mode on GPU %d tile %u to %s\n", d->index, tileId, mode.c_str());
	}

	return result;
}

/**
 * @brief Sets the performance factor for the device.
 *
 * @param d Device information structure.
 *
 * @return ze_result_t Result of the operation.
 */
ze_result_t cmdConfig::setPerformanceFactor(devInfo *d)
{
	TRACING();

	std::string performanceFactor = configCmds[configCmdType::PERFORMANCEFACTOR].val;
	std::vector<std::string> parts = split(performanceFactor, ',');

	if (parts.size() != 2) {
		ERR("Invalid performance factor format. Expected: engineType,factorValue\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	std::string engineType = parts[0];
	std::transform(engineType.begin(), engineType.end(), engineType.begin(),
				   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	if (engineType != "compute" && engineType != "media") {
		ERR("Invalid engine type. Valid options: compute, media\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Validate factor value is numeric (can be decimal)
	char *endPtr = nullptr;
	double factorValue = std::strtod(parts[1].c_str(), &endPtr);
	if (endPtr == parts[1].c_str() || *endPtr != '\0') {
		ERR("Error parsing factor value: invalid numeric format\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (factorValue < 0 || factorValue > 100) {
		ERR("Factor value must be between 0 and 100\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	performance *perf = d->dev->getPerformance();
	if (perf == nullptr) {
		ERR("Error: Performance pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	zes_engine_type_flag_t engineTypeFlag =
		(engineType == "compute") ? ZES_ENGINE_TYPE_FLAG_COMPUTE : ZES_ENGINE_TYPE_FLAG_MEDIA;

	ze_result_t result = perf->setConfig(engineTypeFlag, factorValue);

	if (result == ZE_RESULT_SUCCESS) {
		if (configCmds[configCmdType::TILE].enabled) {
			PRINT("Succeeded in changing the %s performance factor to %.1f on GPU %d tile %s\n", engineType.c_str(),
				  factorValue, d->index, configCmds[configCmdType::TILE].val.c_str());
		} else {
			PRINT("Succeeded in changing the %s performance factor to %.1f on GPU %d\n", engineType.c_str(),
				  factorValue, d->index);
		}
	}

	return result;
}

/**
 * @brief Sets the Xe Link port configuration for the device.
 *
 * @param d Device information structure.
 *
 * @return ze_result_t Result of the operation.
 */
ze_result_t cmdConfig::setXeLinkPort(devInfo *d)
{
	TRACING();

	if (!configCmds[configCmdType::TILE].enabled) {
		ERR("Error: Tile ID is required for XeLink port configuration. Use -t <tileId>.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Validate tile ID
	const std::string &tileIdStr = configCmds[configCmdType::TILE].val;
	if (tileIdStr.empty() || !std::all_of(tileIdStr.begin(), tileIdStr.end(), ::isdigit)) {
		ERR("Error: Invalid tile ID.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	uint32_t tileId = static_cast<uint32_t>(std::stoul(tileIdStr));

	std::string xeLinkPort = configCmds[configCmdType::XELINKPORT].val;
	std::vector<std::string> parts = split(xeLinkPort, ',');

	if (parts.size() != 2) {
		ERR("Invalid XeLink port format. Expected: portId,value\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Validate port ID
	if (parts[0].empty() || !std::all_of(parts[0].begin(), parts[0].end(), ::isdigit)) {
		ERR("Error parsing XeLink port parameters: invalid port ID\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Validate enabled value
	if (parts[1].empty() || (parts[1] != "0" && parts[1] != "1")) {
		ERR("Port value must be 0 (down) or 1 (up)\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	uint32_t portId = static_cast<uint32_t>(std::stoul(parts[0]));
	int enabled = std::stoi(parts[1]);

	fabric *f = d->dev->getFabric();
	if (f == nullptr) {
		ERR("Error: Fabric pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	portInfoSet portSet = {};
	portSet.subdeviceId = tileId;
	portSet.portId.portNumber = static_cast<uint8_t>(portId);
	portSet.enabled = (enabled == 1);
	portSet.settingEnabled = true;
	portSet.settingBeaconing = false;

	ze_result_t result = f->setFabricPorts(d->zesDeviceHdl, portSet);
	if (result == ZE_RESULT_SUCCESS) {
		PRINT("Succeeded in changing Xe Link port %u to %s on GPU %d tile %u\n", portId, enabled ? "up" : "down",
			  d->index, tileId);
	}

	return result;
}

/**
 * @brief Sets the Xe Link port beaconing configuration for the device.
 *
 * @param d Device information structure.
 *
 * @return ze_result_t Result of the operation.
 */
ze_result_t cmdConfig::setXeLinkPortBeaconing(devInfo *d)
{
	TRACING();

	if (!configCmds[configCmdType::TILE].enabled) {
		ERR("Error: Tile ID is required for XeLink beaconing configuration. Use -t <tileId>.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Validate tile ID
	const std::string &tileIdStr = configCmds[configCmdType::TILE].val;
	if (tileIdStr.empty() || !std::all_of(tileIdStr.begin(), tileIdStr.end(), ::isdigit)) {
		ERR("Error: Invalid tile ID.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	uint32_t tileId = static_cast<uint32_t>(std::stoul(tileIdStr));

	std::string xeLinkBeaconing = configCmds[configCmdType::XELINKPORTBEACONING].val;
	std::vector<std::string> parts = split(xeLinkBeaconing, ',');

	if (parts.size() != 2) {
		ERR("Invalid XeLink beaconing format. Expected: portId,value\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Validate port ID
	if (parts[0].empty() || !std::all_of(parts[0].begin(), parts[0].end(), ::isdigit)) {
		ERR("Error parsing XeLink beaconing parameters: invalid port ID\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Validate enabled value
	if (parts[1].empty() || (parts[1] != "0" && parts[1] != "1")) {
		ERR("Beaconing value must be 0 (off) or 1 (on)\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	uint32_t portId = static_cast<uint32_t>(std::stoul(parts[0]));
	int enabled = std::stoi(parts[1]);

	fabric *f = d->dev->getFabric();
	if (f == nullptr) {
		ERR("Error: Fabric pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	portInfoSet portSet = {};
	portSet.subdeviceId = tileId;
	portSet.portId.portNumber = static_cast<uint8_t>(portId);
	portSet.beaconing = (enabled == 1);
	portSet.settingEnabled = false;
	portSet.settingBeaconing = true;

	ze_result_t result = f->setFabricPorts(d->zesDeviceHdl, portSet);
	if (result == ZE_RESULT_SUCCESS) {
		PRINT("Succeeded in changing Xe Link port %u beaconing to %s on GPU %d tile %u\n", portId,
			  enabled ? "on" : "off", d->index, tileId);
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
	int memoryEcc = stoi(configCmds[configCmdType::MEMORYECC].val);
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
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to set memory ECC state: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	zes_device_ecc_state_t requestedState =
		(memoryEcc == 1) ? ZES_DEVICE_ECC_STATE_ENABLED : ZES_DEVICE_ECC_STATE_DISABLED;

	if (state.currentState == requestedState) {
		PRINT("Memory ECC is already %s on GPU %d \n", (memoryEcc == 1 ? "enabled" : "disabled"), d->index);
	} else if (state.pendingState == requestedState) {
		PRINT("Successfully %s ECC memory setting on GPU %d \n", (memoryEcc == 1 ? "enabled" : "disabled"), d->index);
		PRINT("Please perform %s for the change to take effect.\n",
			  e->printEccPendingAction(state.pendingAction).c_str());
	} else {
		ERR("Failed to %s ECC memory setting on GPU %d\n", (memoryEcc == 1 ? "enable" : "disable"), d->index);
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
	int pcieDowngrade = stoi(configCmds[configCmdType::PCIEDOWNGRADE].val);
	if (pcieDowngrade != 0 && pcieDowngrade != 1) {
		ERR("Invalid PCIe downgrade value. Valid options are 0:disable; 1:enable\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Check the PCIe downgrade status using the device class
	bool enabled = (pcieDowngrade == 1);
	PciDowngradeState state = {};
	ze_result_t result = d->dev->getPCI()->getPciDowngradeState(d->zesDeviceHdl, state);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to set PCIe downgrade state: 0x%X (%s)\n", result, l0_error_to_string(result));
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
		ERR("Failed to set PCIe downgrade state: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	PRINT("PCIe downgrade %s successfully. please complete %s for change to take effect\n",
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

	PRINT("Resetting GPU %d. This may take up to one minute...\n", d->index);

	ze_result_t result = d->dev->resetDevice(d->zesDeviceHdl);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to reset device: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	PRINT("Succeeded in resetting the GPU %d\n", d->index);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Applies PPR to the device.
 *
 * @param d Device information structure.
 *
 * @return ze_result_t Result of the operation.
 */
ze_result_t cmdConfig::applyPpr(UNUSED devInfo *d)
{
	TRACING();

	PRINT("Applying PPR to GPU %d. This may take up to ten minutes...\n", d->index);
	PRINT("PPR feature is not available on this platform\n");

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Forces PPR to run on the device.
 *
 * @param d Device information structure.
 *
 * @return ze_result_t Result of the operation.
 */
ze_result_t cmdConfig::forcePpr(UNUSED devInfo *d)
{
	TRACING();
	// This is not implemented for Xe driver in Linux so should we simply return NA going forward?

	PRINT("  PPR: N/A\n");

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
	int opt;
	int optionIndex = 0;
	bool found = false;
	bool isQueryMode = true; // Track if this is a query or set operation
	std::string shortOpts;
	std::vector<struct option> longOptsVec;

	processOptions(configCmds, shortOpts, longOptsVec);
	const struct option *longOpts = longOptsVec.data();
	// Skip the first two arguments (process and command name)
	int startind = 2;
	optind = 2;

	// Parse command line arguments
	while ((opt = GETOPT_LONG(args->argc, args->argv, shortOpts.c_str(), longOpts, &optionIndex)) != -1) {
		switch (opt) {
		case 'h':
			help();
			return 0;
		case 'j':
			configCmds[configCmdType::CONFIGJSON].enabled = true;
			break;
		case 'd':
			configCmds[configCmdType::CONFIGDEVICE].enabled = true;
			configCmds[configCmdType::CONFIGDEVICE].val = optarg;
			break;
		case 't':
			configCmds[configCmdType::TILE].enabled = true;
			configCmds[configCmdType::TILE].val = optarg;
			break;
		case 0:
			for (auto &cmd : configCmds) {
				if (STRCASECMP(longOpts[optionIndex].name, cmd.second.opt.name) == 0) {
					cmd.second.enabled = true;
					if (longOpts[optionIndex].has_arg == required_argument) {
						cmd.second.val = optarg;
					}
					// Any command with a function means this is a set operation
					if (cmd.second.func != nullptr || cmd.first == configCmdType::RESET ||
						cmd.first == configCmdType::PPR || cmd.first == configCmdType::FORCE) {
						isQueryMode = false;
					}
					found = true;
					break;
				}
			}

			if (!found) {
				ERR("The following argument was not expected: '%s'.\n", longOpts[optionIndex].name);
				ERR("Run with --help for more information.\n");
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}

			break;
		default:
			ERR("The following argument was not expected: '%s'.\n", args->argv[startind]);
			ERR("Run with --help for more information.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		startind++;
	}

	// Check for extra arguments
	if (optind != args->argc) {
		ERR("The following argument was not expected: '%s'.\n", args->argv[optind]);
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Check if the device ID is provided
	if (configCmds[configCmdType::CONFIGDEVICE].val.empty()) {
		ERR("Device ID is required.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	result = args->sm.findDevice(configCmds[configCmdType::CONFIGDEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '%s'.\n",
			configCmds[configCmdType::CONFIGDEVICE].val.c_str());
		return result;
	}

	// If this is query mode, display configuration
	if (isQueryMode) {
		for (auto &device : deviceList) {
			displayDeviceConfig(&device);
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
					ERR("Failed to apply configuration to GPU %d, continuing with remaining devices...\n",
						device.index);
				}
			}
		}
	}

	// Return the first error encountered, or success if all succeeded
	return hadError ? firstError : ZE_RESULT_SUCCESS;
}
