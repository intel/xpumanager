/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_log.h"
#include "debug.h"
#include <assert.h>
#include <CLI/CLI.hpp>
#include <filesystem>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdLogs::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Collect GPU debug logs"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s log [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s log -f [fileName]", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "-f,--file                   The file to write all the debug logs into"));

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Executes the logs run.
 *
 * @return int Returns 0 on success.
 */
int cmdLogs::run(arg_struct *args)
{
	TRACING();
	std::string fileName;
	bool jsonOutput = false;
	ze_result_t result;

	CLI::App sub{"Collect GPU debug logs", "log"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", jsonOutput, "Print result in JSON format");
	sub.add_option("-f,--file", fileName, "The file to write all the debug logs into");

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

	// Return an error if the filename wasn't provided
	if (fileName.empty()) {
		ERR("Filename can't be empty\n");
		help();
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	DBG("fileName: {}, jsonOutput: {}", fileName.c_str(), jsonOutput);
	result = args->sm.getLogs(fileName);
	if (result == ZE_RESULT_SUCCESS) {
		if (jsonOutput) {
			PRINT("{{\n    \"status\": \"OK\"\n}}\n");
		} else {
			PRINT("Logs collected successfully in file: {}\n", fileName.c_str());
		}
	} else {
		if (jsonOutput) {
			PRINT("{{\n    \"errno\": {},\n    \"error\": \"Error\"\n}}\n", result);
		} else {
			ERR("Error collecting logs in file: {}, error code: {}\n", fileName.c_str(), result);
		}
	}

	return result;
}
