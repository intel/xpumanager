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

#include "cmd_group.h"
#include "debug.h"

groupCmdStruct groupCmds[] = {
	{groupCmdType::GROUP_HELP, {"help", no_argument, 0, 'h'}},
	{groupCmdType::GROUP_JSON, {"json", no_argument, 0, 'j'}},
	{groupCmdType::GROUP_CREATE, {"create", no_argument, 0, 'c'}},
	{groupCmdType::GROUP_DELETE, {"delete", no_argument, 0, 'D'}},
	{groupCmdType::GROUP_LIST, {"list", no_argument, 0, 'l'}},
	{groupCmdType::GROUP_ADD, {"add", no_argument, 0, 'a'}},
	{groupCmdType::GROUP_REMOVE, {"remove", no_argument, 0, 'r'}},
	{groupCmdType::GROUP_GROUP, {"group", required_argument, 0, 'g'}},
	{groupCmdType::GROUP_NAME1, {"name", required_argument, 0, 'n'}},
	{groupCmdType::GROUP_DEVICE, {"device", required_argument, 0, 'd'}},
};

void cmdGroup::help(HELP helpType)
{
	TRACING();
	vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Group the managed GPU devices"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: xpumcli group [Options]"));
	helpList.push_back(helpCmd(HEADING, "xpumcli group -c -n [groupName]"));
	helpList.push_back(helpCmd(HEADING, "xpumcli group -a -g [groupId] -d [deviceIds]"));
	helpList.push_back(helpCmd(HEADING, "xpumcli group -r -d [deviceIds] -g [groupId]"));
	helpList.push_back(helpCmd(HEADING, "xpumcli group -D -g [groupId]"));
	helpList.push_back(helpCmd(HEADING, "xpumcli group -l"));
	helpList.push_back(helpCmd(HEADING, "xpumcli group -l -g [groupId]"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "-c,--create                 Create a group."));
	helpList.push_back(helpCmd(HEADING, "-D,--delete                 Delete a group."));
	helpList.push_back(helpCmd(HEADING, "-l,--list                   List the groups info."));
	helpList.push_back(helpCmd(HEADING, "-a,--add                    Add devices to a group."));
	helpList.push_back(helpCmd(HEADING, "-r,--remove                 Remove devices from group."));
	helpList.push_back(helpCmd(HEADING, "-g,--group                  Group ID."));
	helpList.push_back(helpCmd(HEADING, "-n,--name                   Group name."));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 Device IDs."));

	printHelp(helpList, helpType);
	helpList.clear();
}

ze_result_t cmdGroup::create(groupCmdStruct *groupCmds, devInfo *d)
{
	TRACING();
	UNUSED(groupCmds);
	UNUSED(d);
	DBG("Creating group...\n");
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdGroup::deleteGroup(groupCmdStruct *groupCmds, devInfo *d)
{
	TRACING();
	UNUSED(groupCmds);
	UNUSED(d);
	DBG("Deleting group...\n");
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdGroup::listGroup(groupCmdStruct *groupCmds, devInfo *d)
{
	TRACING();
	UNUSED(groupCmds);
	UNUSED(d);
	DBG("Listing group...\n");
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdGroup::add(groupCmdStruct *groupCmds, devInfo *d)
{
	TRACING();
	UNUSED(groupCmds);
	UNUSED(d);
	DBG("Adding to group...\n");
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdGroup::remove(groupCmdStruct *groupCmds, devInfo *d)
{
	TRACING();
	UNUSED(groupCmds);
	UNUSED(d);
	DBG("Removing from group...\n");
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the discovery run.
 *
 * @return int Returns 0 on success.
 */
int cmdGroup::run(arg_struct *args)
{
	TRACING();
	devInfo d = {};
	vector<device *> deviceList;
	vector<ze_device_handle_t> deviceHandleList;
	ze_result_t result;
	int opt;
	int optionIndex = 0;
	string shortOpts;
	vector<struct option> longOptsVec;

	processOptions(groupCmds, ARRAY_SIZE(groupCmds), shortOpts, longOptsVec);
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
			groupCmds[groupCmdType::GROUP_JSON].enabled = true;
			break;
		case 'c':
			groupCmds[groupCmdType::GROUP_CREATE].enabled = true;
			break;
		case 'D':
			groupCmds[groupCmdType::GROUP_DELETE].enabled = true;
			break;
		case 'l':
			groupCmds[groupCmdType::GROUP_LIST].enabled = true;
			break;
		case 'a':
			groupCmds[groupCmdType::GROUP_ADD].enabled = true;
			break;
		case 'r':
			groupCmds[groupCmdType::GROUP_REMOVE].enabled = true;
			break;
		case 'g':
			groupCmds[groupCmdType::GROUP_GROUP].enabled = true;
			groupCmds[groupCmdType::GROUP_GROUP].val = optarg;
			break;
		case 'n':
			groupCmds[groupCmdType::GROUP_NAME1].enabled = true;
			groupCmds[groupCmdType::GROUP_NAME1].val = optarg;
			break;
		case 'd':
			groupCmds[groupCmdType::GROUP_DEVICE].enabled = true;
			groupCmds[groupCmdType::GROUP_DEVICE].val = optarg;
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

	result = args->sm.findDevice(groupCmds[groupCmdType::GROUP_DEVICE].val.c_str(), &deviceList, &deviceHandleList);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Error: Device handle not found for device ID '%s'.\n", groupCmds[groupCmdType::GROUP_DEVICE].val.c_str());
		return result;
	}

	int i = 0;
	for (auto &device : deviceList)
	{
		d.dev = device;
		d.deviceHdl = deviceHandleList[i++];
		// Call the appropriate command function based on the command type
		for (auto &cmd : groupCmds)
		{
			if (cmd.enabled && cmd.func != nullptr)
			{
				DBG("Running command: %s\n", cmd.opt.name);
				(this->*cmd.func)(groupCmds, &d);
			}
		}
	}

	return 0;
}
