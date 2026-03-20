/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_group.h"
#include "debug.h"

static std::unordered_map<groupCmdType, groupCmdStruct> groupCmds = {
	{groupCmdType::GROUP_HELP, {{"help", no_argument, 0, 'h'}, nullptr, false, ""}},
	{groupCmdType::GROUP_JSON, {{"json", no_argument, 0, 'j'}, nullptr, false, ""}},
	{groupCmdType::GROUP_CREATE, {{"create", no_argument, 0, 'c'}, &cmdGroup::create, false, ""}},
	{groupCmdType::GROUP_DELETE, {{"delete", no_argument, 0, 'D'}, &cmdGroup::deleteGroup, false, ""}},
	{groupCmdType::GROUP_LIST, {{"list", no_argument, 0, 'l'}, &cmdGroup::listGroup, false, ""}},
	{groupCmdType::GROUP_ADD, {{"add", no_argument, 0, 'a'}, &cmdGroup::add, false, ""}},
	{groupCmdType::GROUP_REMOVE, {{"remove", no_argument, 0, 'r'}, &cmdGroup::remove, false, ""}},
	{groupCmdType::GROUP_GROUP, {{"group", required_argument, 0, 'g'}, nullptr, false, ""}},
	{groupCmdType::GROUP_NAME1, {{"name", required_argument, 0, 'n'}, nullptr, false, ""}},
	{groupCmdType::GROUP_DEVICE, {{"device", required_argument, 0, 'd'}, nullptr, false, ""}},
};

/**
 * @brief Displays help information for the group command
 *
 * This function provides comprehensive help information for the group command,
 * including usage examples, descriptions of options, and subcommand explanations
 * for managing GPU device groups.
 *
 * @param helpType Type of help to display (FULL_HELP or SHORT_HELP)
 */
void cmdGroup::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Group the managed GPU devices"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s group [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s group -c -n [groupName]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s group -a -g [groupId] -d [deviceIds]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s group -r -d [deviceIds] -g [groupId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s group -D -g [groupId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s group -l", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s group -l -g [groupId]", progName.c_str()));
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

/**
 * @brief Creates a new GPU device group
 *
 * This function implements the logic to create a new group for managed GPU devices.
 * Currently implemented as a placeholder for future group creation functionality.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful group creation
 */
ze_result_t cmdGroup::create(UNUSED devInfo *d)
{
	TRACING();
	DBG("Creating group...\n");
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Deletes an existing GPU device group
 *
 * This function implements the logic to delete a specified group of managed GPU devices.
 * Currently implemented as a placeholder for future group deletion functionality.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful group deletion
 */
ze_result_t cmdGroup::deleteGroup(UNUSED devInfo *d)
{
	TRACING();
	DBG("Deleting group...\n");
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Lists all GPU device groups or specific group details
 *
 * This function implements the logic to list all groups or details of a specific group.
 * Currently implemented as a placeholder for future group listing functionality.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful group listing
 */
ze_result_t cmdGroup::listGroup(UNUSED devInfo *d)
{
	TRACING();
	DBG("Listing group...\n");
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Adds a device to a GPU device group
 *
 * This function implements the logic to add a device to an existing group.
 * Currently implemented as a placeholder for future device addition functionality.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful device addition
 */
ze_result_t cmdGroup::add(UNUSED devInfo *d)
{
	TRACING();
	DBG("Adding to group...\n");
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Removes a device from a GPU device group
 *
 * This function implements the logic to remove a device from an existing group.
 * Currently implemented as a placeholder for future device removal functionality.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful device removal
 */
ze_result_t cmdGroup::remove(UNUSED devInfo *d)
{
	TRACING();
	DBG("Removing from group...\n");
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Runs the group command with specified arguments
 *
 * This function parses command line arguments and executes the appropriate group operation
 * (create, delete, list, add, or remove) on the specified GPU devices.
 *
 * @param args Pointer to argument structure containing command line arguments and system manager
 * @return int Returns 0 on success, non-zero on failure
 */
int cmdGroup::run(arg_struct *args)
{
	TRACING();
	std::vector<devInfo> deviceList;
	ze_result_t result;
	int opt;
	int optionIndex = 0;
	std::string shortOpts;
	std::vector<struct option> longOptsVec;

	processOptions(groupCmds, shortOpts, longOptsVec);
	const struct option *longOpts = longOptsVec.data();
	// Skip the first two arguments (process and command name)
	int startind = 2;
	optind = 2;

	while ((opt = getopt_long(args->argc, args->argv, shortOpts.c_str(), longOpts, &optionIndex)) != -1) {
		switch (opt) {
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
			ERR("The following argument was not expected: '{}'.\n", args->argv[startind]);
			ERR("Run with --help for more information.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		startind++;
	}

	// If optind is not equal to args->argc, it means there are extra arguments
	// that were not processed by getopt_long.
	if (optind != args->argc) {
		ERR("The following argument was not expected: '{}'.\n", args->argv[optind]);
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	result = args->sm.findDevice(groupCmds[groupCmdType::GROUP_DEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '{}'.\n", groupCmds[groupCmdType::GROUP_DEVICE].val.c_str());
		return result;
	}

	ze_result_t firstError = ZE_RESULT_SUCCESS;

	// Iterate through the device list and execute the command
	for (auto &device : deviceList) {
		// Call the appropriate command function based on the command type
		for (const auto &cmd : groupCmds) {
			if (cmd.second.enabled && cmd.second.func != nullptr) {
				DBG("Running command: {}\n", cmd.second.opt.name);
				ze_result_t cmdResult = (this->*cmd.second.func)(&device);
				if (cmdResult != ZE_RESULT_SUCCESS && firstError == ZE_RESULT_SUCCESS) {
					firstError = cmdResult;
				}
			}
		}
	}

	return firstError;
}
