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

#include "cmd_stats.h"
#include "debug.h"
#include <assert.h>

statsCmdStruct statsCmds[] = {
	{"eu", &cmdStats::eu},
	{"ras", &cmdStats::ras},
	{"x", &cmdStats::x},
	{"xelink", &cmdStats::xelink},
	{"utils", &cmdStats::utils},
};

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdStats::help(HELP helpType)
{
	TRACING();
	vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "List the GPU statistics"));
	helpList.push_back(helpCmd(TITLE, ""));
	helpList.push_back(helpCmd(TITLE, "Usage: xpu-smi stats [Options]"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [deviceId]"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [pciBdfAddress]"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [deviceId] -j"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [pciBdfAddress] -j"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [deviceId] -e"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [pciBdfAddress] -e"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [deviceId] -e -j"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [pciBdfAddress] -e -j"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [deviceId] -r"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [pciBdfAddress] -r"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [deviceId] -r -j"));
	helpList.push_back(helpCmd(HEADING, "xpu-smi stats -d [pciBdfAddress] -r -j"));
	helpList.push_back(helpCmd(TITLE, ""));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(TITLE, ""));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 The device ID or PCI BDF address to query"));
	helpList.push_back(helpCmd(HEADING, "-e,--eu                     Show EU metrics"));
	helpList.push_back(helpCmd(HEADING, "-r,--ras                    Show RAS error metrics"));
	helpList.push_back(helpCmd(HEADING, "-x                          Show Xe Link metrics"));
	helpList.push_back(helpCmd(HEADING, "--xelink                    Show the all the Xe Link throughput (GB/s) matrix"));
	helpList.push_back(helpCmd(HEADING, "--utils                     Show the Xe Link throughput utilization"));

	printHelp(helpList, helpType);
	helpList.clear();
}

ze_result_t cmdStats::eu(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdStats::ras(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdStats::x(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdStats::xelink(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdStats::utils(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the stats run.
 *
 * @return int Returns 0 on success.
 */
int cmdStats::run(arg_struct *args)
{
	TRACING();
	UNUSED(args);
	return 0;
}
