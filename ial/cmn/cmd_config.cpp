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

#include "cmd_config.h"
#include "debug.h"
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param help_list A pointer to a list of help commands.
 */
void cmdConfig::help(list<help_cmd *> *help_list)
{
	TRACING();
	assert(help_list);

	help_list->push_back(new help_cmd(NO_GAP, "Get and change the GPU settings"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Usage: xpu-smi config [Options]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi config -d [deviceId]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi config -d [deviceId] -t [tileId] --frequencyrange [minFrequency,maxFrequency]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi config -d [deviceId] --powerlimit [powerValue]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi config -d [deviceId] -t [tileId] --standby [standbyMode]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi config -d [deviceId] -t [tileId] --scheduler [schedulerMode]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi config -d [deviceId] -t [tileId] --performancefactor [engineType,factorValue]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi config -d [deviceId] -t [tileId] --xelinkport [portId,value]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi config -d [deviceId] -t [tileId] --xelinkportbeaconing [portId,value]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi config -d [deviceId] --memoryecc [0|1] 0:disable; 1:enable"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi config -d [deviceId] --reset"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi config -d [deviceId] --ppr"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Options:"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(SMALL_GAP, "-d,--device                 The device ID or PCI BDF address to query"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-t,--tile                   The tile ID"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--frequencyrange            GPU tile-level core frequency range"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--powerlimit                Device-level power limit"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--standby                   Tile-level standby mode. Valid options: \"default\"; \"never\""));
	help_list->push_back(new help_cmd(SMALL_GAP, "--scheduler                 Tile-level scheduler mode. Value options: \"timeout\",timeoutValue (us); \"timeslice\",interval (us),yieldtimeout (us);\"exclusive\""));
	help_list->push_back(new help_cmd(LARGE_GAP, "The valid range of all time values (us) is from 5000 to 100,000,000."));
	help_list->push_back(new help_cmd(SMALL_GAP, "--reset                     Reset device by SBR (Secondary Bus Reset)"));
	help_list->push_back(new help_cmd(LARGE_GAP, "For Intel(R) Max Series GPU, when SR-IOV is enabled, please add \"pci=realloc=off\" into Linux kernel command line parameters"));
	help_list->push_back(new help_cmd(LARGE_GAP, "When SR-IOV is disabled, please add \"pci=realloc=on\" into Linux kernel command line parameters"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--ppr                       Apply ppr to the device"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--force                     Force PPR to run"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--performancefactor         Set the tile-level performance factor. Valid options: \"compute/media\";factorValue. The factor value should be"));
	help_list->push_back(new help_cmd(LARGE_GAP, "between 0 to 100. 100 means that the workload is completely compute bounded and requires very little support from the memory support"));
	help_list->push_back(new help_cmd(LARGE_GAP, "0 means that the workload is completely memory bounded and the performance of the memory controller needs to be increased"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--xelinkport                Change the Xe Link port status. The value 0 means down and 1 means up"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--xelinkportbeaconing       Change the Xe Link port beaconing status. The value 0 means off and 1 means on"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--memoryecc                 Enable/disable memory ECC setting. 0:disable; 1:enable"));
}

/**
 * @brief Executes the discovery run.
 *
 * @return int Returns 0 on success.
 */
int cmdConfig::run(sysinfo *sys)
{
	TRACING();
	UNUSED(sys);
	return 0;
}
