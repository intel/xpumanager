/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_log.h"
#include "debug.h"
#include <assert.h>
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
	static struct option longOpts[] = {{"help", no_argument, 0, 'h'},
									   {"json", no_argument, 0, 'j'},
									   {"file", required_argument, 0, 'f'},
									   {0, 0, 0, 0}};

	int opt;
	int optionIndex = 0;
	std::string fileName;
	bool jsonOutput = false;
	ze_result_t result;
	// Skip the first two arguments (process and command name)
	int startind = 2;
	optind = 2;

	while ((opt = getopt_long(args->argc, args->argv, "hjf:", longOpts, &optionIndex)) != -1) {
		switch (opt) {
		case 'h':
			help();
			return ZE_RESULT_SUCCESS;
		case 'j':
			jsonOutput = true;
			break;
		case 'f':
			if (optarg)
				fileName = optarg;
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

	// Return an error if the filename wasn't provided
	if (fileName.empty()) {
		ERR("Filename can't be empty\n");
		help();
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Check to make sure that the file doesn't exist in the current folder
	if (std::filesystem::exists(fileName)) {
		ERR("File '{}' already exists. Please choose a different name or remove the existing file.\n",
			fileName.c_str());
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
