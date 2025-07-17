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

#include "cmd_health.h"
#include "debug.h"
#include <assert.h>
#include <temperature.h>
#include <memory.h>

healthCmdStruct healthCmds[] = {
	{healthCmdType::HEALTH_HELP, {"help", no_argument, 0, 'h'}, nullptr, false, ""},
	{healthCmdType::HEALTH_JSON, {"json", no_argument, 0, 'j'}, nullptr, false, ""},
	{healthCmdType::HEALTH_LIST, {"list", no_argument, 0, 'l'}, nullptr, false, ""},
	{healthCmdType::HEALTH_DEVICE, {"device", required_argument, 0, 'd'}, nullptr, false, ""},
	{healthCmdType::HEALTH_COMPONENT, {"component", required_argument, 0, 'c'}, &cmdHealth::component, false, ""},
};

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdHealth::help(HELP helpType)
{
	TRACING();
	vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Get the GPU device component health status"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s health [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s health -l", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s health -l -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s health -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s health -d [pciBdfAddress]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s health -d [deviceId] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s health -d [pciBdfAddress] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s health -d [deviceId] -c [componentTypeId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s health -d [pciBdfAddress] -c [componentTypeId] -j", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "-l,--list                   Display health info for all devices"));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 The device ID or PCI BDF address"));
	helpList.push_back(helpCmd(HEADING, "-c,--component              Component types"));
	helpList.push_back(helpCmd(SUB_HEADING2, "1. GPU Core Temperature"));
	helpList.push_back(helpCmd(SUB_HEADING2, "2. GPU Memory Temperature"));
	helpList.push_back(helpCmd(SUB_HEADING2, "3. GPU Power"));
	helpList.push_back(helpCmd(SUB_HEADING2, "4. GPU Memory"));
	helpList.push_back(helpCmd(SUB_HEADING2, "5. Xe Link Port"));
	helpList.push_back(helpCmd(SUB_HEADING2, "6. GPU Frequency"));

	printHelp(helpList, helpType);
	helpList.clear();
}

ze_result_t cmdHealth::component(healthCmdStruct *healthCmds, devInfo *d)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	bool found = false;

	healthSubCmdStruct componentCmds[] = {
		{healthSubCmdType::HEALTH_CORETEMPERATURE, &cmdHealth::coreTemperature},
		{healthSubCmdType::HEALTH_MEMORYTEMPERATURE, &cmdHealth::memoryTemperature},
		{healthSubCmdType::HEALTH_POWER, &cmdHealth::power},
		{healthSubCmdType::HEALTH_MEMORY, &cmdHealth::healthMemory},
		{healthSubCmdType::HEALTH_XELINKPORT, &cmdHealth::xeLinkPort},
		{healthSubCmdType::HEALTH_FREQUENCY, &cmdHealth::frequency},
	};

	for (auto &test : componentCmds) {
		if (test.type == atoi(healthCmds[healthCmdType::HEALTH_COMPONENT].val.c_str())) {
			DBG("Running test: %d\n", test.type);
			found = true;
			result = (this->*test.func)(healthCmds, d);
			break;
		}
	}

	if (!found) {
		ERR("The following argument was not expected: '%s'.\n",
			healthCmds[healthCmdType::HEALTH_COMPONENT].val.c_str());
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	return result;
}

ze_result_t cmdHealth::coreTemperature(healthCmdStruct *healthCmds, devInfo *d)
{
	TRACING();
	UNUSED(healthCmds);
	ze_result_t result = ZE_RESULT_SUCCESS;
	uint32_t throttleThreshold = 0;
	uint32_t shutdownThreshold = 0;

	temperature *t = (temperature *)d->dev->getTemperature();
	if (t == nullptr) {
		ERR("Failed to get temperature handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	result = t->getCoreThreshold(d->deviceHdl, &throttleThreshold, &shutdownThreshold);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get core temperature thresholds: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	PRINT("Core Temperature Thresholds:\n");
	PRINT("  Throttle Threshold: %u C\n", throttleThreshold);
	PRINT("  Shutdown Threshold: %u C\n", shutdownThreshold);

	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdHealth::memoryTemperature(healthCmdStruct *healthCmds, devInfo *d)
{
	TRACING();
	UNUSED(healthCmds);
	ze_result_t result = ZE_RESULT_SUCCESS;
	uint32_t throttleThreshold = 0;
	uint32_t shutdownThreshold = 0;

	temperature *t = (temperature *)d->dev->getTemperature();
	if (t == nullptr) {
		ERR("Failed to get temperature handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	result = t->getMemoryThreshold(d->deviceHdl, &throttleThreshold, &shutdownThreshold);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory temperature thresholds: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	PRINT("Memory Temperature Thresholds:\n");
	PRINT("  Throttle Threshold: %u C\n", throttleThreshold);
	PRINT("  Shutdown Threshold: %u C\n", shutdownThreshold);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdHealth::power(healthCmdStruct *healthCmds, devInfo *d)
{
	TRACING();
	UNUSED(healthCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdHealth::healthMemory(healthCmdStruct *healthCmds, devInfo *d)
{
	TRACING();
	UNUSED(healthCmds);
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_mem_health_t health;

	memory *mem = (memory *)d->dev->getMemory();
	if (mem == nullptr) {
		ERR("Failed to get memory handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	result = mem->getMemoryHealth(&health);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory temperature thresholds: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	if (health == ZES_MEM_HEALTH_OK) {
		PRINT("Memory Health: OK\n");
	} else if (health == ZES_MEM_HEALTH_DEGRADED) {
		PRINT("Memory Health: DEGRADED\n");
	} else if (health == ZES_MEM_HEALTH_CRITICAL) {
		PRINT("Memory Health: CRITICAL\n");
	} else if (health == ZES_MEM_HEALTH_REPLACE) {
		PRINT("Memory Health: REPLACE\n");
	} else {
		PRINT("The memory health cannot be determined.\n");
	}
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdHealth::xeLinkPort(healthCmdStruct *healthCmds, devInfo *d)
{
	TRACING();
	UNUSED(healthCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdHealth::frequency(healthCmdStruct *healthCmds, devInfo *d)
{
	TRACING();
	UNUSED(healthCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the health run.
 *
 * @return int Returns 0 on success.
 */
int cmdHealth::run(arg_struct *args)
{
	TRACING();
	UNUSED(args);
	vector<devInfo> deviceList;
	ze_result_t result;
	int opt;
	int optionIndex = 0;
	string shortOpts;
	vector<struct option> longOptsVec;

	processOptions(healthCmds, ARRAY_SIZE(healthCmds), shortOpts, longOptsVec);
	const struct option *longOpts = longOptsVec.data();
	// Skip the first two arguments (process and command name)
	int startind = 2;
	optind = 2;

	while ((opt = getopt_long(args->argc, args->argv, shortOpts.c_str(), longOpts, &optionIndex)) != -1) {
		switch (opt) {
		case 'h':
			help();
			return ZE_RESULT_SUCCESS;
		case 'j':
			healthCmds[healthCmdType::HEALTH_JSON].enabled = true;
			break;
		case 'l':
			healthCmds[healthCmdType::HEALTH_LIST].enabled = true;
			break;
		case 'd':
			healthCmds[healthCmdType::HEALTH_DEVICE].enabled = true;
			healthCmds[healthCmdType::HEALTH_DEVICE].val = optarg;
			break;
		case 'c':
			healthCmds[healthCmdType::HEALTH_COMPONENT].enabled = true;
			healthCmds[healthCmdType::HEALTH_COMPONENT].val = optarg;
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

	// If no options were specified, print help
	if (args->argc == 2) {
		help();
		return ZE_RESULT_SUCCESS;
	}

	// user must specify a device ID or PCI BDF address
	if (!healthCmds[healthCmdType::HEALTH_LIST].enabled && !healthCmds[healthCmdType::HEALTH_DEVICE].enabled) {
		ERR("You must specify a device ID or PCI BDF address with the -d option.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Find the device based on the provided device ID or PCI BDF address
	result = args->sm.findDevice(healthCmds[healthCmdType::HEALTH_DEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '%s'.\n",
			healthCmds[healthCmdType::HEALTH_DEVICE].val.c_str());
		return result;
	}

	// Iterate through the device list and execute the command
	for (auto &device : deviceList) {
		// Call the appropriate command function based on the command type
		for (auto &cmd : healthCmds) {
			if (cmd.enabled && cmd.func != nullptr) {
				DBG("Running command: %s\n", cmd.opt.name);
				result = (this->*cmd.func)(healthCmds, &device);
				break;
			}
		}
	}

	return result;
}
