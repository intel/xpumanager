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

#include "cmd_stats.h"
#include "debug.h"
#include <assert.h>

statsCmdStruct statsCmds[] = {
	{STATS_HELP, {"help", no_argument, 0, 'h'}},
	{STATS_JSON, {"json", no_argument, 0, 'j'}},
	{STATS_DEVICE, {"device", required_argument, 0, 'd'}},
	{STATS_EU, {"eu", no_argument, 0, 'e'}, &cmdStats::eu},
	{STATS_RAS, {"ras", no_argument, 0, 'r'}, &cmdStats::ras},
	{STATS_X, {"x", no_argument, 0, 'x'}, &cmdStats::x},
	{STATS_XELINK, {"xelink", no_argument, 0, 0}, &cmdStats::xelink},
	{STATS_UTILS, {"utils", no_argument, 0, 0}, &cmdStats::utils},
};

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdStats::help(HELP helpType)
{
	TRACING();
	vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "List the GPU statistics"));
	helpList.push_back(helpCmd(TITLE, ""));
	helpList.push_back(helpCmd(TITLE, "Usage: xpu-smi stats [Options]"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [deviceId]"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [pciBdfAddress]"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [deviceId] -j"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [pciBdfAddress] -j"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [deviceId] -e"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [pciBdfAddress] -e"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [deviceId] -e -j"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [pciBdfAddress] -e -j"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [deviceId] -r"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [pciBdfAddress] -r"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [deviceId] -r -j"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [pciBdfAddress] -r -j"));
	helpList.push_back(helpCmd(TITLE, ""));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(TITLE, ""));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 The device ID or PCI BDF address to query"));
	helpList.push_back(helpCmd(HEADING, "-e,--eu                     Show EU metrics"));
	helpList.push_back(helpCmd(HEADING, "-r,--ras                    Show RAS error metrics"));
	helpList.push_back(helpCmd(HEADING, "-x                          Show Xe Link metrics"));
	helpList.push_back(helpCmd(HEADING, "--xelink                    Show the all the Xe Link throughput (GB/s) matrix"));
	helpList.push_back(helpCmd(HEADING, "--utils                     Show the Xe Link throughput utilization"));

	printHelp(helpList, helpType);
	helpList.clear();
}

ze_result_t cmdStats::eu(statsCmdStruct *statsCmds, devInfo *d)
{
	TRACING();
	UNUSED(statsCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdStats::ras(statsCmdStruct *statsCmds, devInfo *d)
{
	TRACING();
	UNUSED(statsCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdStats::x(statsCmdStruct *statsCmds, devInfo *d)
{
	TRACING();
	UNUSED(statsCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdStats::xelink(statsCmdStruct *statsCmds, devInfo *d)
{
	TRACING();
	UNUSED(statsCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdStats::utils(statsCmdStruct *statsCmds, devInfo *d)
{
	TRACING();
	UNUSED(statsCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the stats run.
 *
 * @return int Returns 0 on success.
 */
int cmdStats::run(arg_struct *args)
{
	TRACING();
	devInfo d = {};
	vector<device *> deviceList;
	vector<ze_device_handle_t> deviceHandleList;
	ze_result_t result;
	bool found = false;
	int opt;
	int optionIndex = 0;
	string shortOpts;
	vector<struct option> longOptsVec;

	processOptions(statsCmds, ARRAY_SIZE(statsCmds), shortOpts, longOptsVec);
	const struct option *longOpts = longOptsVec.data();
	// Skip the first two arguments (process and command name)
	int startind = 2;
	optind = 2;

	while ((opt = getopt_long(args->argc, args->argv, shortOpts.c_str(), longOpts, &optionIndex)) != -1)
	{
		switch (opt)
		{
		case 'h':
			help();
			return 0;
		case 'j':
			statsCmds[STATS_JSON].enabled = true;
			break;
		case 'd':
			statsCmds[STATS_DEVICE].enabled = true;
			statsCmds[STATS_DEVICE].val = optarg;
			break;
		case 'e':
			statsCmds[STATS_EU].enabled = true;
			break;
		case 'r':
			statsCmds[STATS_RAS].enabled = true;
			break;
		case 'x':
			statsCmds[STATS_X].enabled = true;
			break;
		case 0:
			for (auto &cmd : statsCmds)
			{
				if (STRCASECMP(longOpts[optionIndex].name, cmd.opt.name) == 0)
				{
					statsCmds[cmd.type].enabled = true;
					if (longOpts[optionIndex].has_arg == required_argument)
					{
						statsCmds[cmd.type].val = optarg;
					}
					found = true;
					break;
				}
			}

			if (!found)
			{
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
	if (optind != args->argc)
	{
		ERR("The following argument was not expected: '%s'.\n", args->argv[optind]);
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	result = args->sm.findDevice(statsCmds[STATS_DEVICE].val.c_str(), &deviceList, &deviceHandleList);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Error: Device handle not found for device ID '%s'.\n", statsCmds[STATS_DEVICE].val.c_str());
		return result;
	}

	int i = 0;
	for (auto &device : deviceList)
	{
		d.dev = device;
		d.deviceHdl = deviceHandleList[i++];
		// Call the appropriate command function based on the command type
		for (auto &cmd : statsCmds)
		{
			if (cmd.enabled && cmd.func != nullptr)
			{
				DBG("Running command: %s\n", cmd.opt.name);
				result = (this->*cmd.func)(statsCmds, &d);
				break;
			}
		}
	}
	return result;
}
