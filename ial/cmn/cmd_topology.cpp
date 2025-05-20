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
void cmdTopology::help(HELP helpType)
{
	TRACING();
	vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Get the system topology"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s topology [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s topology -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s topology -d [pciBdfAddress]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s topology -d [deviceId] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s topology -f [filename]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s topology -m", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 The device ID or PCI BDF address to query"));
	helpList.push_back(helpCmd(HEADING, "-f,--file                   Generate the system topology with the GPU info to a XML file"));
	helpList.push_back(helpCmd(HEADING, "-m,--matrix                 Print the CPU/GPU topology matrix"));
	helpList.push_back(helpCmd(SUB_HEADING, "S: Self"));
	helpList.push_back(helpCmd(SUB_HEADING, "XL[laneCount]: Two tiles on the different cards are directly connected by Xe Link.  Xe Link LAN count is also provided"));
	helpList.push_back(helpCmd(SUB_HEADING, "XL*: Two tiles on the different cards are connected by Xe Link + MDF. They are not directly connected by Xe Link"));
	helpList.push_back(helpCmd(SUB_HEADING, "SYS: Connected with PCIe between NUMA nodes"));
	helpList.push_back(helpCmd(SUB_HEADING, "NODE: Connected with PCIe within a NUMA node"));
	helpList.push_back(helpCmd(SUB_HEADING, "MDF: Connected with Multi-Die Fabric Interface"));

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Executes the topology run.
 *
 * @return int Returns 0 on success.
 */
int cmdTopology::run(arg_struct *args)
{
	TRACING();
	UNUSED(args);
	return 0;
}
