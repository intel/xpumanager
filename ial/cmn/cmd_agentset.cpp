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

#include "cmd_agentset.h"
#include "debug.h"

void cmdAgentSet::help(HELP helpType)
{
	TRACING();
	vector<helpCmd> helpList;
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

int cmdAgentSet::run(arg_struct *args)
{
	TRACING();
	static struct option longOptions[] = {
		{"help", no_argument, 0, 'h'},
		{"json", no_argument, 0, 'j'},
		{"list", no_argument, 0, 'l'},
		{"time", required_argument, 0, 't'},
		{0, 0, 0, 0},
	};

	int opt;
	int optionIndex = 0;
	bool showJson = false;
	bool showList = false;
	int timeInterval = -1;

	// Skip the first two arguments (process and command name)
	int startind = 2;
	optind = 2;

	while ((opt = getopt_long(args->argc, args->argv, "hjlt:", longOptions, &optionIndex)) != -1) {
		switch (opt) {
		case 'h':
			help();
			return ZE_RESULT_SUCCESS;
		case 'j':
			showJson = true;
			break;
		case 'l':
			showList = true;
			break;
		case 't':
			if (optarg) {
				timeInterval = atoi(optarg);
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
