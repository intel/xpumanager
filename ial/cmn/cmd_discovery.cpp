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

#include "cmd_discovery.h"
#include "debug.h"
#include <assert.h>

discoveryCmdStruct discoveryCmds[] = {
	{"dump", &cmdDiscovery::dump},
	{"listamcversions", &cmdDiscovery::listamcversions},
};

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdDiscovery::help(list<helpCmd *> *helpList)
{
	TRACING();
	assert(helpList);
	helpList->push_back(new helpCmd(NO_GAP, "Discover the GPU devices installed on this machine and provide the device info"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Usage: xpu-smi discovery [Options]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi discovery"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi discovery -d [deviceId]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi discovery -d [pciBdfAddress]"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi discovery -d [deviceId] -j"));
	helpList->push_back(new helpCmd(SMALL_GAP, "xpu-smi discovery --dump [propertyIds]"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(NO_GAP, "Options:"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	helpList->push_back(new helpCmd(NO_GAP, ""));
	helpList->push_back(new helpCmd(SMALL_GAP, "-d,--device                 Device ID or PCI BDF address to query. It will show more detailed info"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--pf,--physicalfunction     Display the physical functions only"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--vf,--virtualfunction      Display the virtual functions only"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--dump                      Property ID to dump device properties in CSV format. Separated by the comma. \"-1\" means all properties"));
	helpList->push_back(new helpCmd(LARGE_GAP, "1. Device ID"));
	helpList->push_back(new helpCmd(LARGE_GAP, "2. Device Name"));
	helpList->push_back(new helpCmd(LARGE_GAP, "3. Vendor Name"));
	helpList->push_back(new helpCmd(LARGE_GAP, "4. SOC UUID"));
	helpList->push_back(new helpCmd(LARGE_GAP, "5. Serial Number"));
	helpList->push_back(new helpCmd(LARGE_GAP, "6. Core Clock Rate"));
	helpList->push_back(new helpCmd(LARGE_GAP, "7. Stepping"));
	helpList->push_back(new helpCmd(LARGE_GAP, "8. Driver Version"));
	helpList->push_back(new helpCmd(LARGE_GAP, "9. GFX Firmware Version"));
	helpList->push_back(new helpCmd(LARGE_GAP, "10. GFX Data Firmware Version"));
	helpList->push_back(new helpCmd(LARGE_GAP, "11. PCI BDF Address"));
	helpList->push_back(new helpCmd(LARGE_GAP, "12. PCI Slot"));
	helpList->push_back(new helpCmd(LARGE_GAP, "13. PCIe Generation"));
	helpList->push_back(new helpCmd(LARGE_GAP, "14. PCIe Max Link Width"));
	helpList->push_back(new helpCmd(LARGE_GAP, "15. OAM Socket ID"));
	helpList->push_back(new helpCmd(LARGE_GAP, "16. Memory Physical Size"));
	helpList->push_back(new helpCmd(LARGE_GAP, "17. Number of Memory Channels"));
	helpList->push_back(new helpCmd(LARGE_GAP, "18. Memory Bus Width"));
	helpList->push_back(new helpCmd(LARGE_GAP, "19. Number of EUs"));
	helpList->push_back(new helpCmd(LARGE_GAP, "20. Number of Media Engines"));
	helpList->push_back(new helpCmd(LARGE_GAP, "21. Number of Media Enhancement Engines"));
	helpList->push_back(new helpCmd(LARGE_GAP, "22. GFX Firmware Status"));
	helpList->push_back(new helpCmd(LARGE_GAP, "23. PCI Vendor ID"));
	helpList->push_back(new helpCmd(LARGE_GAP, "24. PCI Device ID"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--listamcversions           Show all AMC firmware versions"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-u,--username               Username used to authenticate for host redfish access"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-p,--password               Password used to authenticate for host redfish access"));
	helpList->push_back(new helpCmd(SMALL_GAP, "-y,--assumeyes              Assume that the answer to any question which would be asked is yes"));
}

ze_result_t cmdDiscovery::dump(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::listamcversions(char *subcmd, char *args)
{
	TRACING();
	UNUSED(subcmd);
	UNUSED(args);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the discovery run.
 *
 * @return int Returns 0 on success.
 */
int cmdDiscovery::run(arg_struct *args)
{
	TRACING();
	UNUSED(args);
	return 0;
}
