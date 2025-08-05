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

#include "cmd_oob.h"
#include "debug.h"
#include <assert.h>

/**
 * @brief Displays help information for the out-of-band management command
 *
 * This function shows help text and usage information for OOB management commands.
 *
 * @param helpType Type of help to display (e.g., brief, detailed)
 */
void cmdOOB::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Out-of-Band (OOB) Management Commands"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s oob [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob discovery", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob discovery -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob discovery -d [pciBdfAddress]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob discovery -d [deviceId] -j", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "%s oob telemetry", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob telemetry -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob telemetry -d [pciBdfAddress]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob telemetry -d [deviceId] -j", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "%s oob updatefw fw_type", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob updatefw -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob updatefw -d [pciBdfAddress]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob updatefw -d [deviceId] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob updatefw -t AMC -f [imageFilePath]", progName.c_str()));

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Executes the out-of-band management command with specified arguments
 *
 * This function runs OOB management operations based on the provided command line arguments.
 * Currently implemented as a placeholder for future OOB functionality.
 *
 * @param args Pointer to argument structure containing command line arguments (currently unused)
 * @return int Returns 0 on success, non-zero on failure
 */
int cmdOOB::run(UNUSED arg_struct *args)
{
	TRACING();
	// Implement the command logic here
	return 0;
}