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

discoveryCmdStruct discCmds[] = {
	{discCmdType::DISC_HELP, {"help", no_argument, 0, 'h'}},
	{discCmdType::DISC_JSON, {"json", no_argument, 0, 'j'}},
	{discCmdType::DISC_DEVICE, {"device", required_argument, 0, 'd'}, &cmdDiscovery::dev},
	{discCmdType::DISC_PHYSICALFUNCTION, {"physicalFunction", no_argument, 0, 0}, &cmdDiscovery::physicalFunction},
	{discCmdType::DISC_VIRTUALFUNCTION, {"virtualFunction", no_argument, 0, 0}, &cmdDiscovery::virtualFunction},
	{discCmdType::DISC_DUMP, {"dump", required_argument, 0, 0}, &cmdDiscovery::dump},
	{discCmdType::DISC_LISTAMCVERSIONS, {"listamcversions", no_argument, 0, 0}, &cmdDiscovery::listamcversions},
	{discCmdType::DISC_USERNAME, {"username", required_argument, 0, 'u'}},
	{discCmdType::DISC_PASSWORD, {"password", required_argument, 0, 'p'}},
	{discCmdType::DISC_ASSUMEYES, {"assumeyes", no_argument, 0, 'y'}},
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
	helpList->push_back(new helpCmd(SMALL_GAP, "--pf,--physicalFunction     Display the physical functions only"));
	helpList->push_back(new helpCmd(SMALL_GAP, "--vf,--virtualFunction      Display the virtual functions only"));
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

ze_result_t cmdDiscovery::dev(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::dump(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;

	discoveryDumpStruct dumpCmds[] = {
		{DUMP_DEVICEID, &cmdDiscovery::deviceID},
		{DUMP_DEVICENAME, &cmdDiscovery::deviceName},
		{DUMP_VENDORNAME, &cmdDiscovery::vendorName},
		{DUMP_SOCUUID, &cmdDiscovery::socUuid},
		{DUMP_SERIALNUMBER, &cmdDiscovery::serialNumber},
		{DUMP_CORECLOCKRATE, &cmdDiscovery::coreClockRate},
		{DUMP_STEPPING, &cmdDiscovery::stepping},
		{DUMP_DRIVERVERSION, &cmdDiscovery::driverVersion},
		{DUMP_GFXFIRMWAREVERSION, &cmdDiscovery::gfxFirmwareVersion},
		{DUMP_GFXDATAFIRMWAREVERSION, &cmdDiscovery::gfxDataFirmwareVersion},
		{DUMP_PCIBDFADDRESS, &cmdDiscovery::pciBDFAddress},
		{DUMP_PCISLOT, &cmdDiscovery::pciSlot},
		{DUMP_PCIEGENERATION, &cmdDiscovery::pcieGeneration},
		{DUMP_PCIEMAXLINKWIDTH, &cmdDiscovery::pcieMaxLinkWidth},
		{DUMP_OAMSOCID, &cmdDiscovery::oamSocketID},
		{DUMP_MEMORYPHYSICALSIZE, &cmdDiscovery::memoryPhysicalSize},
		{DUMP_MEMORYCHANNELS, &cmdDiscovery::memoryChannels},
		{DUMP_MEMORYBUSWIDTH, &cmdDiscovery::memoryBusWidth},
		{DUMP_EUS, &cmdDiscovery::eus},
		{DUMP_MEDIAENGINES, &cmdDiscovery::mediaEngines},
		{DUMP_MEDIAENHANCEMENTENGINES, &cmdDiscovery::mediaEnhancementEngines},
		{DUMP_GFXFIRMWARESTATUS, &cmdDiscovery::gfxFirmwareStatus},
		{DUMP_PCIVENDORID, &cmdDiscovery::pciVendorID},
		{DUMP_PCIDEVICEID, &cmdDiscovery::pciDeviceID},
	};

	for (auto &cmd : dumpCmds)
	{
		if (cmd.type == discCmds[discCmdType::DISC_DUMP].type)
		{
			DBG("Running command: %d\n", cmd.type);
			result = (this->*cmd.func)(discCmds, d);
			break;
		}
	}

	return result;
}

ze_result_t cmdDiscovery::deviceID(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::deviceName(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::vendorName(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::socUuid(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::serialNumber(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::coreClockRate(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::stepping(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::driverVersion(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::gfxFirmwareVersion(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::gfxDataFirmwareVersion(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::pciBDFAddress(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::pciSlot(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::pcieGeneration(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::pcieMaxLinkWidth(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::oamSocketID(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::memoryPhysicalSize(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::memoryChannels(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::memoryBusWidth(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::eus(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::mediaEngines(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::mediaEnhancementEngines(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::gfxFirmwareStatus(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::pciVendorID(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::pciDeviceID(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::physicalFunction(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::virtualFunction(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::listamcversions(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
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
	devInfo d = {};
	vector<device *> deviceList;
	vector<ze_device_handle_t> deviceHandleList;
	ze_result_t result;
	bool found = false;
	int opt;
	int optionIndex = 0;
	string shortOpts;
	vector<struct option> longOptsVec;

	processOptions(discCmds, ARRAY_SIZE(discCmds), shortOpts, longOptsVec);
	const struct option *longOpts = longOptsVec.data();

	while ((opt = getopt_long(args->argc, args->argv, shortOpts.c_str(), longOpts, &optionIndex)) != -1)
	{
		switch (opt)
		{
		case 'h':
			return 0;
		case 'j':
			discCmds[discCmdType::DISC_JSON].enabled = true;
			break;
		case 'd':
			discCmds[discCmdType::DISC_DEVICE].enabled = true;
			discCmds[discCmdType::DISC_DEVICE].val = optarg;
			break;
		case 'u':
			discCmds[discCmdType::DISC_USERNAME].enabled = true;
			discCmds[discCmdType::DISC_USERNAME].val = optarg;
			break;
		case 'p':
			discCmds[discCmdType::DISC_PASSWORD].enabled = true;
			discCmds[discCmdType::DISC_PASSWORD].val = optarg;
			break;
		case 'y':
			discCmds[discCmdType::DISC_ASSUMEYES].enabled = true;
			break;
		case 0:
			for (auto &cmd : discCmds)
			{
				if (STRCASECMP(longOpts[optionIndex].name, cmd.opt.name) == 0)
				{
					discCmds[cmd.type].enabled = true;
					if (longOpts[optionIndex].has_arg == required_argument)
					{
						discCmds[cmd.type].val = optarg;
					}
					found = true;
					break;
				}
			}

			if (!found)
			{
				ERR("Unknown command: %s\n", longOpts[optionIndex].name);
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}

			break;
		default:
			// Handle invalid options
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	result = args->sm.findDeviceByBDF(discCmds[discCmdType::DISC_DEVICE].val.c_str(), &deviceList, &deviceHandleList);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Error: Device handle not found for device ID '%s'.\n", discCmds[discCmdType::DISC_DEVICE].val.c_str());
		return result;
	}

	int i = 0;
	for (auto &device : deviceList)
	{
		d.dev = device;
		d.deviceHdl = deviceHandleList[i++];
		// Call the appropriate command function based on the command type
		for (auto &cmd : discCmds)
		{
			if (cmd.enabled && cmd.func != nullptr)
			{
				DBG("Running command: %s\n", cmd.opt.name);
				(this->*cmd.func)(discCmds, &d);
			}
		}
	}
	return 0;
}
