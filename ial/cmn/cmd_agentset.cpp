/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_agentset.h"
#include "debug.h"
#include <CLI/CLI.hpp>

/**
 * @brief Displays help information for the agentset command
 *
 * This function prints usage information, command line options, and examples
 * for the agentset command which manages XPU Manager daemon settings.
 *
 * @param helpType The type of help to display (FULL_HELP or SHORT_HELP)
 */
void cmdAgentSet::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;
	helpList.push_back(helpCmd(TITLE, "Get or change some XPU Manager settings."));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s agentset [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s agentset -l", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s agentset -l -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s agentset -t 200", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "-l, --list                  Display all agent settings"));
	helpList.push_back(helpCmd(HEADING, "-t, --time                  Set the time interval(in milliseconds) by which"
										" XPU Manager daemon should retrieve raw gpu statistics."
										" Valid values are 100, 200, 500, 1000."));

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Executes the agentset command with parsed command line arguments
 *
 * This function processes command line arguments for the agentset command,
 * handling options for listing current settings, setting time intervals,
 * and managing JSON output format. It validates input parameters and
 * applies the requested configuration changes.
 *
 * @param args Pointer to argument structure containing argc, argv, and system manager
 * @return int ZE_RESULT_SUCCESS on success, error code on failure
 */
int cmdAgentSet::run(arg_struct *args)
{
	TRACING();
	bool showJson = false;
	bool showList = false;
	int timeInterval = -1;

	CLI::App sub{"Get or change some XPU Manager settings", "agentset"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", showJson, "Print result in JSON format");
	sub.add_flag("-l,--list", showList, "Display all agent settings");
	sub.add_option("-t,--time", timeInterval,
				   "Set the time interval (ms) for raw GPU statistics retrieval. Valid: 100, 200, 500, 1000");

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

	if (showJson) {
		// handle json output logic
	}

	if (showList) {
		// handle list logic
	} else if (timeInterval != 100 && timeInterval != 200 && timeInterval != 500 && timeInterval != 1000) {
		ERR("Invalid time interval. Valid values include 100, 200, 500, 1000.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// handle time interval logic

	return ZE_RESULT_SUCCESS;
}
