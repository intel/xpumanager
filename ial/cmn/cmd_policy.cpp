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

#include "cmd_policy.h"
#include "debug.h"

policyCmdStruct policyCmds[] = {
	{POLICY_HELP, {"help", no_argument, 0, 'h'}},
	{POLICY_JSON, {"json", no_argument, 0, 'j'}},
	{POLICY_DEVICE, {"device", required_argument, 0, 'd'}},
	{POLICY_GROUP, {"group", required_argument, 0, 'g'}},
	{POLICY_LIST, {"list", no_argument, 0, 'l'}, &cmdPolicy::listPolicies},
	{POLICY_LISTALLTYPES, {"listalltypes", no_argument, 0, 0}, &cmdPolicy::listTypes},
	{POLICY_CREATE, {"create", no_argument, 0, 'c'}, &cmdPolicy::create},
	{POLICY_REMOVE, {"remove", no_argument, 0, 'r'}, &cmdPolicy::remove},
	{POLICY_TYPE, {"type", required_argument, 0, 0}},
	{POLICY_CONDITION, {"condition", required_argument, 0, 0}},
	{POLICY_THRESHOLD, {"threshold", required_argument, 0, 0}},
	{POLICY_ACTION, {"action", required_argument, 0, 0}},
	{POLICY_THROTTLEFREQUENCYMIN, {"throttlefrequencymin", required_argument, 0, 0}},
	{POLICY_THROTTLEFREQUENCYMAX, {"throttlefrequencymax", required_argument, 0, 0}},
};

void cmdPolicy::help(HELP helpType)
{
	TRACING();
	vector<helpCmd> helpList;
	helpList.push_back(helpCmd(TITLE, "Get and set the GPU policies."));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s policy [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s policy -l", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s policy --listalltypes", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s policy -d [deviceId] -l", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s policy -d [deviceId] -l -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s policy -g [groupId] -l", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s policy -g [groupId] -l -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING,
							   "%s policy -c -d [deviceId] --type 1 --condition 1"
							   " --threshold [threshold]  --action 1 --throttlefrequencymin [frequencyMinValue]"
							   " --throttlefrequencymax [frequencyMaxValue]",
							   progName.c_str()));
	helpList.push_back(helpCmd(HEADING,
							   "%s policy -c -g [groupId] --type 1 --condition 1 --threshold [threshold]"
							   "  --action 1 --throttlefrequencymin [frequencyMinValue]"
							   " --throttlefrequencymax [frequencyMaxValue]",
							   progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s policy -r -d [deviceId] --type [policyTypeValue]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s policy -r -g [groupId] --type [policyTypeValue]", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 The device ID or PCI BDF address"));
	helpList.push_back(helpCmd(HEADING, "-g,--group                  The group ID."));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "-l,--list                   List all policies."));
	helpList.push_back(helpCmd(
		HEADING, "--listalltypes              List all policy types, including the supported condition and action."));
	helpList.push_back(helpCmd(HEADING, "-c,--create                 Create one policy."));
	helpList.push_back(helpCmd(HEADING, "-r,--remove                 Remove one policy. Only the policy is removed and "
										"the changed GPU settings will not be resumed."));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "--type                      Policy types."));
	helpList.push_back(helpCmd(SUB_HEADING2, "1. GPU Core Temperature"));
	helpList.push_back(helpCmd(HEADING, "--condition                 Conditions."));
	helpList.push_back(helpCmd(SUB_HEADING2, "1. More than"));
	helpList.push_back(helpCmd(HEADING, "--threshold                 Threshold"));
	helpList.push_back(helpCmd(HEADING, "--action                    Policy action."));
	helpList.push_back(helpCmd(SUB_HEADING2, "1. Throttle GPU"));
	helpList.push_back(helpCmd(HEADING, "--throttlefrequencymin      Throttle GPU frequency to min value"));
	helpList.push_back(helpCmd(HEADING, "--throttlefrequencymax      Throttle GPU frequency to max value"));

	printHelp(helpList, helpType);
	helpList.clear();
}

ze_result_t cmdPolicy::create(policyCmdStruct *policyCmds, devInfo *d)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	UNUSED(policyCmds);
	UNUSED(d);

	return result;
}

ze_result_t cmdPolicy::listPolicies(policyCmdStruct *policyCmds, devInfo *d)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	UNUSED(policyCmds);
	UNUSED(d);

	return result;
}

ze_result_t cmdPolicy::listTypes(policyCmdStruct *policyCmds, devInfo *d)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	UNUSED(policyCmds);
	UNUSED(d);

	return result;
}

ze_result_t cmdPolicy::remove(policyCmdStruct *policyCmds, devInfo *d)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	UNUSED(policyCmds);
	UNUSED(d);

	return result;
}

int cmdPolicy::run(arg_struct *args)
{
	TRACING();
	vector<devInfo> deviceList;
	ze_result_t result;
	bool found = false;
	int opt;
	int optionIndex = 0;
	string shortOpts;
	vector<struct option> longOptsVec;

	processOptions(policyCmds, ARRAY_SIZE(policyCmds), shortOpts, longOptsVec);
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
			policyCmds[POLICY_JSON].enabled = true;
			break;
		case 'd':
			policyCmds[POLICY_DEVICE].enabled = true;
			policyCmds[POLICY_DEVICE].val = optarg;
			break;
		case 'g':
			policyCmds[POLICY_GROUP].enabled = true;
			policyCmds[POLICY_GROUP].val = optarg;
			break;
		case 'l':
			policyCmds[POLICY_LIST].enabled = true;
			break;
		case 'c':
			policyCmds[POLICY_CREATE].enabled = true;
			break;
		case 'r':
			policyCmds[POLICY_REMOVE].enabled = true;
			break;
		case 0:
			for (auto &cmd : policyCmds) {
				if (STRCASECMP(longOpts[optionIndex].name, cmd.opt.name) == 0) {
					policyCmds[cmd.type].enabled = true;
					if (longOpts[optionIndex].has_arg == required_argument) {
						policyCmds[cmd.type].val = optarg;
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

	result = args->sm.findDevice(policyCmds[POLICY_DEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '%s'.\n", policyCmds[POLICY_DEVICE].val.c_str());
		return result;
	}

	// Iterate through the device list and execute the command
	for (auto &device : deviceList) {
		// Call the appropriate command function based on the command type
		for (auto &cmd : policyCmds) {
			if (cmd.enabled && cmd.func != nullptr) {
				DBG("Running command: %s\n", cmd.opt.name);
				result = (this->*cmd.func)(policyCmds, &device);
				break;
			}
		}
	}
	return result;
}
