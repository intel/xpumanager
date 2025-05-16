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

#include "cmd_log.h"
#include "debug.h"
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdLogs::help(HELP helpType)
{
	TRACING();
	vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Collect GPU debug logs"));
	helpList.push_back(helpCmd(TITLE, ""));
	helpList.push_back(helpCmd(TITLE, "Usage: xpu-smi log [Options]"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi log -f [tarGzipFileName]"));
	helpList.push_back(helpCmd(TITLE, ""));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(TITLE, ""));
	helpList.push_back(helpCmd(HEADING, "-f,--file                   The file (a tar.gz) to archive all the debug logs"));

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
	UNUSED(args);
	return 0;
}
