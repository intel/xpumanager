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

healthCmdStruct healthCmds[] = {
	{healthCmdType::HEALTH_HELP, {"help", no_argument, 0, 'h'}},
	{healthCmdType::HEALTH_JSON, {"json", no_argument, 0, 'j'}},
	{healthCmdType::HEALTH_LIST, {"list", no_argument, 0, 'l'}},
	{healthCmdType::HEALTH_DEVICE, {"device", required_argument, 0, 'd'}},
	{healthCmdType::HEALTH_COMPONENT, {"component", required_argument, 0, 'c'}, &cmdHealth::component},
};

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdHealth::help(list<helpCmd *> *helpList)
{
	TRACING();
	assert(helpList);
	helpList->push_back(new helpCmd(NO_GAP, "Get the GPU device component health status"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Usage: xpu-smi health [Options]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi health -l"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi health -l -j"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi health -d [deviceId]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi health -d [pciBdfAddress]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi health -d [deviceId] -j"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi health -d [pciBdfAddress] -j"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi health -d [deviceId] -c [componentTypeId]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi health -d [pciBdfAddress] -c [componentTypeId] -j"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Options:"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(SMALL_GAP, "-l,--list                   Display health info for all devices"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-d,--device                 The device ID or PCI BDF address"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-c,--component              Component types"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "1. GPU Core Temperature"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "2. GPU Memory Temperature"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "3. GPU Power"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "4. GPU Memory"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "5. Xe Link Port"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "6. GPU Frequency"));
}

ze_result_t cmdHealth::component(healthCmdStruct *healthCmds, devInfo *d)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;

	healthSubCmdStruct componentCmds[] = {
		{healthSubCmdType::HEALTH_CORETEMPERATURE, &cmdHealth::coreTemperature},
		{healthSubCmdType::HEALTH_MEMORYTEMPERATURE, &cmdHealth::memoryTemperature},
		{healthSubCmdType::HEALTH_POWER, &cmdHealth::power},
		{healthSubCmdType::HEALTH_MEMORY, &cmdHealth::memory},
		{healthSubCmdType::HEALTH_XELINKPORT, &cmdHealth::xeLinkPort},
		{healthSubCmdType::HEALTH_FREQUENCY, &cmdHealth::frequency},
	};

	for (auto &test : componentCmds)
	{
		if (test.type == healthCmds[healthCmdType::HEALTH_COMPONENT].type)
		{
			DBG("Running test: %d\n", test.type);
			result = (this->*test.func)(healthCmds, d);
			break;
		}
	}

	return result;
}

ze_result_t cmdHealth::coreTemperature(healthCmdStruct *healthCmds, devInfo *d)
{
	TRACING();
	UNUSED(healthCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdHealth::memoryTemperature(healthCmdStruct *healthCmds, devInfo *d)
{
	TRACING();
	UNUSED(healthCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdHealth::power(healthCmdStruct *healthCmds, devInfo *d)
{
	TRACING();
	UNUSED(healthCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdHealth::memory(healthCmdStruct *healthCmds, devInfo *d)
{
	TRACING();
	UNUSED(healthCmds);
	UNUSED(d);
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
	devInfo d = {};
	vector<device *> deviceList;
	vector<ze_device_handle_t> deviceHandleList;
	ze_result_t result;
	int opt;
	int optionIndex = 0;
	string shortOpts;
	vector<struct option> longOptsVec;

	processOptions(healthCmds, ARRAY_SIZE(healthCmds), shortOpts, longOptsVec);
	const struct option *longOpts = longOptsVec.data();

	while ((opt = getopt_long(args->argc, args->argv, shortOpts.c_str(), longOpts, &optionIndex)) != -1)
	{
		switch (opt)
		{
		case 'h':
			help(nullptr);
			return 0;
		case 'j':
			healthCmds[healthCmdType::HEALTH_JSON].enabled = true;
			break;
		case 'l':
			healthCmds[healthCmdType::HEALTH_LIST].enabled = true;
			return 0;
		case 'd':
			healthCmds[healthCmdType::HEALTH_DEVICE].enabled = true;
			healthCmds[healthCmdType::HEALTH_DEVICE].val = optarg;
			break;
		case 'c':
			healthCmds[healthCmdType::HEALTH_COMPONENT].enabled = true;
			healthCmds[healthCmdType::HEALTH_COMPONENT].val = optarg;
			break;
		default:
			ERR("Unknown command: %s\n", longOpts[optionIndex].name);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	result = args->sm.findDeviceByBDF(healthCmds[healthCmdType::HEALTH_DEVICE].val.c_str(),
									  &deviceList, &deviceHandleList);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Error: Device handle not found for device ID '%s'.\n",
			healthCmds[healthCmdType::HEALTH_DEVICE].val.c_str());
		return result;
	}

	int i = 0;
	for (auto &device : deviceList)
	{
		d.dev = device;
		d.deviceHdl = deviceHandleList[i++];
		// Call the appropriate command function based on the command type
		for (auto &cmd : healthCmds)
		{
			if (cmd.enabled && cmd.func != nullptr)
			{
				DBG("Running command: %s\n", cmd.opt.name);
				result = (this->*cmd.func)(healthCmds, &d);
				break;
			}
		}
	}

	return result;
}
