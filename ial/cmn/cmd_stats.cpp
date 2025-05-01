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

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param help_list A pointer to a list of help commands.
 */
void cmdStats::help(list<help_cmd *> *help_list)
{
	TRACING();
	assert(help_list);
	help_list->push_back(new help_cmd(NO_GAP, "List the GPU statistics"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Usage: xpu-smi stats [Options]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [deviceId]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [pciBdfAddress]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [deviceId] -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [pciBdfAddress] -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [deviceId] -e"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [pciBdfAddress] -e"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [deviceId] -e -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [pciBdfAddress] -e -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [deviceId] -r"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [pciBdfAddress] -r"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [deviceId] -r -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi stats -d [pciBdfAddress] -r -j"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Options:"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(SMALL_GAP, "-d,--device                 The device ID or PCI BDF address to query"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-e,--eu                     Show EU metrics"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-r,--ras                    Show RAS error metrics"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-x                          Show Xe Link metrics"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--xelink                    Show the all the Xe Link throughput (GB/s) matrix"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--utils                     Show the Xe Link throughput utilization"));
}

/**
 * @brief Executes the stats run.
 *
 * @return int Returns 0 on success.
 */
int cmdStats::run(sysinfo *sys)
{
	TRACING();
	UNUSED(sys);
	return 0;
}
