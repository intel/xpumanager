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
 * @param helpList A pointer to a list of help commands.
 */
void cmdConfig::help(list<helpCmd *> *helpList)
{
	TRACING();
	assert(helpList);

	helpList->push_back(new helpCmd(NO_GAP, "Get and change the GPU settings"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Usage: xpu-smi config [Options]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi config -d [deviceId]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi config -d [deviceId] -t [tileId] --frequencyrange [minFrequency,maxFrequency]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi config -d [deviceId] --powerlimit [powerValue]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi config -d [deviceId] -t [tileId] --standby [standbyMode]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi config -d [deviceId] -t [tileId] --scheduler [schedulerMode]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi config -d [deviceId] -t [tileId] --performancefactor [engineType,factorValue]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi config -d [deviceId] -t [tileId] --xelinkport [portId,value]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi config -d [deviceId] -t [tileId] --xelinkportbeaconing [portId,value]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi config -d [deviceId] --memoryecc [0|1] 0:disable; 1:enable"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi config -d [deviceId] --reset"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi config -d [deviceId] --ppr"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Options:"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(SMALL_GAP, "-d,--device                 The device ID or PCI BDF address to query"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-t,--tile                   The tile ID"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--frequencyrange            GPU tile-level core frequency range"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--powerlimit                Device-level power limit"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--standby                   Tile-level standby mode. Valid options: \"default\"; \"never\""));
	helpList->push_back(new helpCmd(SMALL_GAP, "--scheduler                 Tile-level scheduler mode. Value options: \"timeout\",timeoutValue (us); \"timeslice\",interval (us),yieldtimeout (us);\"exclusive\""));
	helpList->push_back(new helpCmd(LARGE_GAP, "The valid range of all time values (us) is from 5000 to 100,000,000."));
	helpList->push_back(new helpCmd(SMALL_GAP, "--reset                     Reset device by SBR (Secondary Bus Reset)"));
	helpList->push_back(new helpCmd(LARGE_GAP, "For Intel(R) Max Series GPU, when SR-IOV is enabled, please add \"pci=realloc=off\" into Linux kernel command line parameters"));
	helpList->push_back(new helpCmd(LARGE_GAP, "When SR-IOV is disabled, please add \"pci=realloc=on\" into Linux kernel command line parameters"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--ppr                       Apply ppr to the device"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--force                     Force PPR to run"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--performancefactor         Set the tile-level performance factor. Valid options: \"compute/media\";factorValue. The factor value should be"));
	helpList->push_back(new helpCmd(LARGE_GAP, "between 0 to 100. 100 means that the workload is completely compute bounded and requires very little support from the memory support"));
	helpList->push_back(new helpCmd(LARGE_GAP, "0 means that the workload is completely memory bounded and the performance of the memory controller needs to be increased"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--xelinkport                Change the Xe Link port status. The value 0 means down and 1 means up"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--xelinkportbeaconing       Change the Xe Link port beaconing status. The value 0 means off and 1 means on"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--memoryecc                 Enable/disable memory ECC setting. 0:disable; 1:enable"));
}

/**
 * @brief Executes the discovery run.
 *
 * @return int Returns 0 on success.
 */
int cmdConfig::run()
{
	TRACING();
	return 0;
}
