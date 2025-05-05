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

#include "cmd_topology.h"
#include "debug.h"
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdTopology::help(list<helpCmd *> *helpList)
{
	TRACING();
	assert(helpList);
	helpList->push_back(new helpCmd(NO_GAP, "Get the system topology"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Usage: xpu-smi topology [Options]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi topology -d [deviceId]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi topology -d [pciBdfAddress]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi topology -d [deviceId] -j"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi topology -f [filename]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi topology -m"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Options:"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(SMALL_GAP, "-d,--device                 The device ID or PCI BDF address to query"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-f,--file                   Generate the system topology with the GPU info to a XML file"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-m,--matrix                 Print the CPU/GPU topology matrix"));
	helpList->push_back(new helpCmd(LARGE_GAP, "S: Self"));
	helpList->push_back(new helpCmd(LARGE_GAP, "XL[laneCount]: Two tiles on the different cards are directly connected by Xe Link.  Xe Link LAN count is also provided"));
	helpList->push_back(new helpCmd(LARGE_GAP, "XL*: Two tiles on the different cards are connected by Xe Link + MDF. They are not directly connected by Xe Link"));
	helpList->push_back(new helpCmd(LARGE_GAP, "SYS: Connected with PCIe between NUMA nodes"));
	helpList->push_back(new helpCmd(LARGE_GAP, "NODE: Connected with PCIe within a NUMA node"));
	helpList->push_back(new helpCmd(LARGE_GAP, "MDF: Connected with Multi-Die Fabric Interface"));
}

/**
 * @brief Executes the topology run.
 *
 * @return int Returns 0 on success.
 */
int cmdTopology::run()
{
	TRACING();
	return 0;
}
