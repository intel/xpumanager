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

#include "cmd_health.h"
#include "debug.h"
#include <assert.h>

healthCmdStruct healthCmds[] = {
	{"1", &cmdHealth::coreTemperature},
	{"2", &cmdHealth::memoryTemperature},
	{"3", &cmdHealth::power},
	{"4", &cmdHealth::memory},
	{"5", &cmdHealth::xeLinkPort},
	{"6", &cmdHealth::frequency},
};

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdHealth::help(list<helpCmd *> *helpList)
{
	TRACING();
	assert(helpList);
	helpList->push_back(new helpCmd(NO_GAP, "Get the GPU device component health status"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Usage: xpu-smi health [Options]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi health -l"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi health -l -j"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi health -d [deviceId]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi health -d [pciBdfAddress]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi health -d [deviceId] -j"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi health -d [pciBdfAddress] -j"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi health -d [deviceId] -c [componentTypeId]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi health -d [pciBdfAddress] -c [componentTypeId] -j"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Options:"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(SMALL_GAP, "-l,--list                   Display health info for all devices"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-d,--device                 The device ID or PCI BDF address"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-c,--component              Component types"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "1. GPU Core Temperature"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "2. GPU Memory Temperature"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "3. GPU Power"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "4. GPU Memory"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "5. Xe Link Port"));
	helpList->push_back(new helpCmd(XLARGE_GAP, "6. GPU Frequency"));
}

ze_result_t cmdHealth::coreTemperature(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdHealth::memoryTemperature(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdHealth::power(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdHealth::memory(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdHealth::xeLinkPort(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdHealth::frequency(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the health run.
 *
 * @return int Returns 0 on success.
 */
int cmdHealth::run(arg_struct *args)
{
	TRACING();
	UNUSED(args);
	return 0;
}
