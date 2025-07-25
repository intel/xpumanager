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

#include "cmd_config.h"
#include "debug.h"
#include <assert.h>
#include <ecc.h>
#include <fabric.h>
#include <frequency.h>
#include <power.h>
#include <scheduler.h>
#include <standby.h>

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
	{configCmdType::RESET, {{"reset", no_argument, 0, 0}, &cmdConfig::resetDevice, false, ""}},
	{configCmdType::PPR, {{"ppr", no_argument, 0, 0}, &cmdConfig::applyPpr, false, ""}},
	{configCmdType::FORCE, {{"force", no_argument, 0, 0}, &cmdConfig::forcePpr, false, ""}},
};

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
										"options: \"compute/media\";factorValue. The factor value should be"));
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

	printHelp(helpList, helpType);
	helpList.clear();
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

	ze_result_t result;

	// Parse the frequency range from the option string
	std::string rangeStr = configCmds[configCmdType::FREQUENCYRANGE].val;
	size_t commaPos = rangeStr.find(',');

	if (commaPos == std::string::npos) {
		ERR("Invalid frequency range format. Expected format: minFrequency,maxFrequency\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	std::string minFreqStr = rangeStr.substr(0, commaPos);
	std::string maxFreqStr = rangeStr.substr(commaPos + 1);
	float minFreq = stof(minFreqStr);
	float maxFreq = stof(maxFreqStr);

	if (minFreq < 0 || maxFreq < 0 || minFreq >= maxFreq) {
		ERR("Invalid frequency range values. Min frequency must be less than max frequency"
			" and both must be non-negative.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	frequency *fq = (frequency *)d->dev->getFrequency();
	if (fq == nullptr) {
		ERR("Error: Frequency pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	result = fq->setRange(minFreq, maxFreq);

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
	ze_result_t result;
	// Parse the power limit from the option string.
	float powerLimit = stof(configCmds[configCmdType::POWERLIMIT].val);
	if (powerLimit < 0) {
		ERR("Invalid power limit value. Power limit must be non-negative.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	power *pwr = (power *)d->dev->getPower();
	if (pwr == nullptr) {
		ERR("Error: Power pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	// Set the power limit using the power class
	result = pwr->setPowerLimit(powerLimit);

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

	// Set standby mode. Valid options are default and never
	std::string standbyMode = configCmds[configCmdType::STANDBYMODE].val;
	if (standbyMode != "default" && standbyMode != "never") {
		ERR("Invalid standby mode. Valid options are default and never.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Set the standby mode using the device class
	standby *stby = (standby *)d->dev->getStandby();
	if (stby == nullptr) {
		ERR("Error: Standby pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	ze_result_t result =
		stby->setMode(standbyMode == "default" ? ZES_STANDBY_PROMO_MODE_DEFAULT : ZES_STANDBY_PROMO_MODE_NEVER);
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
	ze_result_t result = ZE_RESULT_SUCCESS;

	// Set scheduler mode. Valid options are  \"timeout\",timeoutValue (us) or
	// \"timeslice\",interval (us),yieldtimeout (us) or \"exclusive\"
	std::string schedulerMode = configCmds[configCmdType::SCHEDULERMODE].val;
	size_t commaPos = schedulerMode.find(',');
	if (commaPos == std::string::npos && schedulerMode != "exclusive") {
		ERR("Invalid scheduler mode format. Expected format: mode,timeoutValue (us)\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	scheduler *sched = (scheduler *)d->dev->getScheduler();
	if (sched == nullptr) {
		ERR("Error: Scheduler pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	std::string timeoutValueStr = schedulerMode.substr(commaPos + 1);
	float timeoutValue = stof(timeoutValueStr);

	if (schedulerMode == "timeout") {
		result = sched->setTimeoutMode(timeoutValue);
	} else if (schedulerMode == "timeslice") {
		size_t secondCommaPos = timeoutValueStr.find(',', commaPos + 1);
		if (secondCommaPos == std::string::npos) {
			ERR("Invalid scheduler mode format. Expected format: mode,timeoutValue (us)\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		std::string intervalStr = timeoutValueStr.substr(0, secondCommaPos);
		std::string yieldTimeoutStr = timeoutValueStr.substr(secondCommaPos + 1);
		float interval = stof(intervalStr);
		float yieldTimeout = stof(yieldTimeoutStr);

		// Valid values are between 5000 to 100,000,000.
		if (interval < 5000 || yieldTimeout < 5000 || interval > 100000000 || yieldTimeout > 100000000) {
			ERR("Invalid scheduler mode values. Valid range is between 5000 to 100,000,000.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		result = sched->setTimesliceMode(interval, yieldTimeout);
	} else {
		result = sched->setExclusiveMode();
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
ze_result_t cmdConfig::setPerformanceFactor(UNUSED devInfo *d)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
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
	// Set Xe Link port. Valid options are 0:disable; 1:enable
	int enable = stoi(configCmds[configCmdType::XELINKPORT].val);
	if (enable != 0 && enable != 1) {
		ERR("Invalid Xe Link port value. Valid options are 0:disable; 1:enable\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	// Set the Xe Link port using the device class
	fabric *f = (fabric *)d->dev->getFabric();
	if (f == nullptr) {
		ERR("Error: Fabric pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	ze_result_t result = f->setPortConfig(enable == 1 ? true : false);

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
	// Set Xe Link port beaconing. Valid options are 0:disable; 1:enable
	int enable = stoi(configCmds[configCmdType::XELINKPORTBEACONING].val);
	if (enable != 0 && enable != 1) {
		ERR("Invalid Xe Link port beaconing value. Valid options are 0:disable; 1:enable\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	// Set the Xe Link port beaconing using the device class
	fabric *f = (fabric *)d->dev->getFabric();
	if (f == nullptr) {
		ERR("Error: Fabric pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	ze_result_t result = f->setPortBeaconing(enable == 1 ? true : false);

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
	ecc *e = (ecc *)d->dev->getECC();
	if (e == nullptr) {
		ERR("Error: ECC pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	ze_result_t result = e->setState(d->deviceHdl, memoryEcc);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to set memory ECC state: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
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
	ze_result_t result = d->dev->resetDevice(d->zesDeviceHdl);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to reset device: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
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
	// This is not implemented for Xe driver in Linux so should we simply return NA going forward?

	PRINT("  PPR: N/A\n");

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
	configCmdType cmdType = configCmdType::TOTAL_CONFIG;
	ze_result_t result;
	int opt;
	int optionIndex = 0;
	bool found = false;
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

	// If optind is not equal to args->argc, it means there are extra arguments
	// that were not processed by getopt_long.
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

	// Iterate through the device list and execute the command
	for (auto &device : deviceList) {
		// Call the appropriate command function based on the command type
		for (const auto &cmd : configCmds) {
			if (cmd.first == cmdType && cmd.second.func != nullptr) {
				result = (this->*cmd.second.func)(&device);
				break;
			}
		}
	}

	return result;
}
