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

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param help_list A pointer to a list of help commands.
 */
void cmdDiscovery::help(list<help_cmd *> *help_list)
{
	TRACING();
	assert(help_list);
	help_list->push_back(new help_cmd(NO_GAP, "Discover the GPU devices installed on this machine and provide the device info"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Usage: xpu-smi discovery [Options]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi discovery"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi discovery -d [deviceId]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi discovery -d [pciBdfAddress]"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi discovery -d [deviceId] -j"));
	help_list->push_back(new help_cmd(SMALL_GAP, "xpu-smi discovery --dump [propertyIds]"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(NO_GAP, "Options:"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-h,--help                   Print this help message and exit"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-j,--json                   Print result in JSON format"));
	help_list->push_back(new help_cmd(NO_GAP, ""));
	help_list->push_back(new help_cmd(SMALL_GAP, "-d,--device                 Device ID or PCI BDF address to query. It will show more detailed info"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--pf,--physicalfunction     Display the physical functions only"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--vf,--virtualfunction      Display the virtual functions only"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--dump                      Property ID to dump device properties in CSV format. Separated by the comma. \"-1\" means all properties"));
	help_list->push_back(new help_cmd(LARGE_GAP, "1. Device ID"));
	help_list->push_back(new help_cmd(LARGE_GAP, "2. Device Name"));
	help_list->push_back(new help_cmd(LARGE_GAP, "3. Vendor Name"));
	help_list->push_back(new help_cmd(LARGE_GAP, "4. SOC UUID"));
	help_list->push_back(new help_cmd(LARGE_GAP, "5. Serial Number"));
	help_list->push_back(new help_cmd(LARGE_GAP, "6. Core Clock Rate"));
	help_list->push_back(new help_cmd(LARGE_GAP, "7. Stepping"));
	help_list->push_back(new help_cmd(LARGE_GAP, "8. Driver Version"));
	help_list->push_back(new help_cmd(LARGE_GAP, "9. GFX Firmware Version"));
	help_list->push_back(new help_cmd(LARGE_GAP, "10. GFX Data Firmware Version"));
	help_list->push_back(new help_cmd(LARGE_GAP, "11. PCI BDF Address"));
	help_list->push_back(new help_cmd(LARGE_GAP, "12. PCI Slot"));
	help_list->push_back(new help_cmd(LARGE_GAP, "13. PCIe Generation"));
	help_list->push_back(new help_cmd(LARGE_GAP, "14. PCIe Max Link Width"));
	help_list->push_back(new help_cmd(LARGE_GAP, "15. OAM Socket ID"));
	help_list->push_back(new help_cmd(LARGE_GAP, "16. Memory Physical Size"));
	help_list->push_back(new help_cmd(LARGE_GAP, "17. Number of Memory Channels"));
	help_list->push_back(new help_cmd(LARGE_GAP, "18. Memory Bus Width"));
	help_list->push_back(new help_cmd(LARGE_GAP, "19. Number of EUs"));
	help_list->push_back(new help_cmd(LARGE_GAP, "20. Number of Media Engines"));
	help_list->push_back(new help_cmd(LARGE_GAP, "21. Number of Media Enhancement Engines"));
	help_list->push_back(new help_cmd(LARGE_GAP, "22. GFX Firmware Status"));
	help_list->push_back(new help_cmd(LARGE_GAP, "23. PCI Vendor ID"));
	help_list->push_back(new help_cmd(LARGE_GAP, "24. PCI Device ID"));
	help_list->push_back(new help_cmd(SMALL_GAP, "--listamcversions           Show all AMC firmware versions"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-u,--username               Username used to authenticate for host redfish access"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-p,--password               Password used to authenticate for host redfish access"));
	help_list->push_back(new help_cmd(SMALL_GAP, "-y,--assumeyes              Assume that the answer to any question which would be asked is yes"));
}

/**
 * @brief Executes the discovery run.
 *
 * @return int Returns 0 on success.
 */
int cmdDiscovery::run()
{
	TRACING();
	return 0;
}
