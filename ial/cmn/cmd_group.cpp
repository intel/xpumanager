/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_group.h"
#include "debug.h"
#include <CLI/CLI.hpp>

static std::unordered_map<groupCmdType, groupCmdStruct> groupCmds = {
	{groupCmdType::GROUP_HELP, {}},
	{groupCmdType::GROUP_JSON, {}},
	{groupCmdType::GROUP_CREATE, {.func = &cmdGroup::create}},
	{groupCmdType::GROUP_DELETE, {.func = &cmdGroup::deleteGroup}},
	{groupCmdType::GROUP_LIST, {.func = &cmdGroup::listGroup}},
	{groupCmdType::GROUP_ADD, {.func = &cmdGroup::add}},
	{groupCmdType::GROUP_REMOVE, {.func = &cmdGroup::remove}},
	{groupCmdType::GROUP_GROUP, {}},
	{groupCmdType::GROUP_NAME1, {}},
	{groupCmdType::GROUP_DEVICE, {}},
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

	// Reset state
	for (auto &[k, v] : groupCmds) {
		v.enabled = false;
		v.val.clear();
	}

	CLI::App sub{"Manage GPU groups", "group"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", groupCmds[groupCmdType::GROUP_JSON].enabled, "Print result in JSON format");
	sub.add_flag("-c,--create", groupCmds[groupCmdType::GROUP_CREATE].enabled, "Create a new group");
	sub.add_flag("-D,--delete", groupCmds[groupCmdType::GROUP_DELETE].enabled, "Delete a group");
	sub.add_flag("-l,--list", groupCmds[groupCmdType::GROUP_LIST].enabled, "List all groups");
	sub.add_flag("-a,--add", groupCmds[groupCmdType::GROUP_ADD].enabled, "Add device to group");
	sub.add_flag("-r,--remove", groupCmds[groupCmdType::GROUP_REMOVE].enabled, "Remove device from group");
	sub.add_option("-g,--group", groupCmds[groupCmdType::GROUP_GROUP].val, "Group ID")->each([&](const std::string &) {
		groupCmds[groupCmdType::GROUP_GROUP].enabled = true;
	});
	sub.add_option("-n,--name", groupCmds[groupCmdType::GROUP_NAME1].val, "Group name")->each([&](const std::string &) {
		groupCmds[groupCmdType::GROUP_NAME1].enabled = true;
	});
	sub.add_option("-d,--device", groupCmds[groupCmdType::GROUP_DEVICE].val, "Device ID or PCI BDF address")
		->each([&](const std::string &) { groupCmds[groupCmdType::GROUP_DEVICE].enabled = true; });

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
				DBG("Running command: {}\n", groupCmdName(cmd.first));
				ze_result_t cmdResult = (this->*cmd.second.func)(&device);
				if (cmdResult != ZE_RESULT_SUCCESS && firstError == ZE_RESULT_SUCCESS) {
					firstError = cmdResult;
				}
			}
		}
	}

	return firstError;
}
