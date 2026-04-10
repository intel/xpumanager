/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_policy.h"
#include "debug.h"
#include <CLI/CLI.hpp>

static std::unordered_map<policyCmdType, policyCmdStruct> policyCmds = {
	{POLICY_HELP, {}},
	{POLICY_JSON, {}},
	{POLICY_DEVICE, {}},
	{POLICY_GROUP, {}},
	{POLICY_LIST, {.func = &cmdPolicy::listPolicies}},
	{POLICY_LISTALLTYPES, {.func = &cmdPolicy::listTypes}},
	{POLICY_CREATE, {.func = &cmdPolicy::create}},
	{POLICY_REMOVE, {.func = &cmdPolicy::remove}},
	{POLICY_TYPE, {}},
	{POLICY_CONDITION, {}},
	{POLICY_THRESHOLD, {}},
	{POLICY_ACTION, {}},
	{POLICY_THROTTLEFREQUENCYMIN, {}},
	{POLICY_THROTTLEFREQUENCYMAX, {}},
};

void cmdPolicy::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;
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

/**
 * @brief Creates a new GPU policy
 *
 * This function implements the logic to create a new policy for GPU management.
 * Currently implemented as a placeholder for future policy creation functionality.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful policy creation
 */
ze_result_t cmdPolicy::create(UNUSED devInfo *d)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;

	return result;
}

/**
 * @brief Lists all existing GPU policies
 *
 * This function retrieves and displays all configured GPU policies.
 * Currently implemented as a placeholder for future policy listing functionality.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful policy retrieval
 */
ze_result_t cmdPolicy::listPolicies(UNUSED devInfo *d)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;

	return result;
}

/**
 * @brief Lists all available GPU policy types
 *
 * This function retrieves and displays all available policy types that can be
 * configured for GPU management. Currently implemented as a placeholder.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful policy type retrieval
 */
ze_result_t cmdPolicy::listTypes(UNUSED devInfo *d)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;

	return result;
}

/**
 * @brief Removes an existing GPU policy
 *
 * This function implements the logic to remove an existing GPU policy.
 * Currently implemented as a placeholder for future policy removal functionality.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful policy removal
 */
ze_result_t cmdPolicy::remove(UNUSED devInfo *d)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;

	return result;
}

/**
 * @brief Runs the policy command with specified arguments
 *
 * This function parses command line arguments and executes the appropriate policy operation
 * (create, list policies, list types, or remove) on the specified GPU devices.
 *
 * @param args Pointer to argument structure containing command line arguments and system manager
 * @return int Returns 0 on success, non-zero on failure
 */
int cmdPolicy::run(arg_struct *args)
{
	TRACING();
	std::vector<devInfo> deviceList;
	ze_result_t result;

	// Reset state
	for (auto &[k, v] : policyCmds) {
		v.enabled = false;
		v.val.clear();
	}

	CLI::App sub{"Manage GPU policies", "policy"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", policyCmds[POLICY_JSON].enabled, "Print result in JSON format");
	sub.add_option("-d,--device", policyCmds[POLICY_DEVICE].val, "Device ID or PCI BDF address")
		->each([&](const std::string &) { policyCmds[POLICY_DEVICE].enabled = true; });
	sub.add_option("-g,--group", policyCmds[POLICY_GROUP].val, "Group ID")->each([&](const std::string &) {
		policyCmds[POLICY_GROUP].enabled = true;
	});
	sub.add_flag("-l,--list", policyCmds[POLICY_LIST].enabled, "List all policies");
	sub.add_flag("--listalltypes", policyCmds[POLICY_LISTALLTYPES].enabled, "List all policy types");
	sub.add_flag("-c,--create", policyCmds[POLICY_CREATE].enabled, "Create a new policy");
	sub.add_flag("-r,--remove", policyCmds[POLICY_REMOVE].enabled, "Remove a policy");
	sub.add_option("--type", policyCmds[POLICY_TYPE].val, "Policy type")->each([&](const std::string &) {
		policyCmds[POLICY_TYPE].enabled = true;
	});
	sub.add_option("--condition", policyCmds[POLICY_CONDITION].val, "Policy condition")->each([&](const std::string &) {
		policyCmds[POLICY_CONDITION].enabled = true;
	});
	sub.add_option("--threshold", policyCmds[POLICY_THRESHOLD].val, "Policy threshold")->each([&](const std::string &) {
		policyCmds[POLICY_THRESHOLD].enabled = true;
	});
	sub.add_option("--action", policyCmds[POLICY_ACTION].val, "Policy action")->each([&](const std::string &) {
		policyCmds[POLICY_ACTION].enabled = true;
	});
	sub.add_option("--throttlefrequencymin", policyCmds[POLICY_THROTTLEFREQUENCYMIN].val, "Minimum throttle frequency")
		->each([&](const std::string &) { policyCmds[POLICY_THROTTLEFREQUENCYMIN].enabled = true; });
	sub.add_option("--throttlefrequencymax", policyCmds[POLICY_THROTTLEFREQUENCYMAX].val, "Maximum throttle frequency")
		->each([&](const std::string &) { policyCmds[POLICY_THROTTLEFREQUENCYMAX].enabled = true; });

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

	result = args->sm.findDevice(policyCmds[POLICY_DEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '{}'.\n", policyCmds[POLICY_DEVICE].val.c_str());
		return result;
	}

	// Iterate through the device list and execute the command
	for (auto &device : deviceList) {
		// Call the appropriate command function based on the command type
		for (const auto &cmd : policyCmds) {
			if (cmd.second.enabled && cmd.second.func != nullptr) {
				DBG("Running command: {}\n", policyCmdName(cmd.first));
				result = (this->*cmd.second.func)(&device);
				break;
			}
		}
	}
	return result;
}
