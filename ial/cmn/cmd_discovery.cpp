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
#include <enginegroup.h>
#include <firmware.h>
#include <memory.h>
#include <pci.h>
#include <sstream>

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
void cmdDiscovery::help(HELP helpType)
{
	TRACING();
	vector<helpCmd> helpList;

	helpList.push_back(
		helpCmd(TITLE, "Discover the GPU devices installed on this machine and provide the device info"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s discovery [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s discovery", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s discovery -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s discovery -d [pciBdfAddress]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s discovery -d [deviceId] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s discovery --dump [propertyIds]", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(
		HEADING, "-d,--device                 Device ID or PCI BDF address to query. It will show more detailed info"));
	helpList.push_back(helpCmd(HEADING, "--pf,--physicalFunction     Display the physical functions only"));
	helpList.push_back(helpCmd(HEADING, "--vf,--virtualFunction      Display the virtual functions only"));
	helpList.push_back(helpCmd(HEADING, "--dump                      Property ID to dump device properties in CSV "
										"format. Separated by the comma. \"-1\" means all properties"));
	helpList.push_back(helpCmd(SUB_HEADING, "1. Device ID"));
	helpList.push_back(helpCmd(SUB_HEADING, "2. Device Name"));
	helpList.push_back(helpCmd(SUB_HEADING, "3. Vendor Name"));
	helpList.push_back(helpCmd(SUB_HEADING, "4. SOC UUID"));
	helpList.push_back(helpCmd(SUB_HEADING, "5. Serial Number"));
	helpList.push_back(helpCmd(SUB_HEADING, "6. Core Clock Rate"));
	helpList.push_back(helpCmd(SUB_HEADING, "7. Stepping"));
	helpList.push_back(helpCmd(SUB_HEADING, "8. Driver Version"));
	helpList.push_back(helpCmd(SUB_HEADING, "9. GFX Firmware Version"));
	helpList.push_back(helpCmd(SUB_HEADING, "10. GFX Data Firmware Version"));
	helpList.push_back(helpCmd(SUB_HEADING, "11. PCI BDF Address"));
	helpList.push_back(helpCmd(SUB_HEADING, "12. PCI Slot"));
	helpList.push_back(helpCmd(SUB_HEADING, "13. PCIe Generation"));
	helpList.push_back(helpCmd(SUB_HEADING, "14. PCIe Max Link Width"));
	helpList.push_back(helpCmd(SUB_HEADING, "15. OAM Socket ID"));
	helpList.push_back(helpCmd(SUB_HEADING, "16. Memory Physical Size"));
	helpList.push_back(helpCmd(SUB_HEADING, "17. Number of Memory Channels"));
	helpList.push_back(helpCmd(SUB_HEADING, "18. Memory Bus Width"));
	helpList.push_back(helpCmd(SUB_HEADING, "19. Number of EUs"));
	helpList.push_back(helpCmd(SUB_HEADING, "20. Number of Media Engines"));
	helpList.push_back(helpCmd(SUB_HEADING, "21. Number of Media Enhancement Engines"));
	helpList.push_back(helpCmd(SUB_HEADING, "22. GFX Firmware Status"));
	helpList.push_back(helpCmd(SUB_HEADING, "23. PCI Vendor ID"));
	helpList.push_back(helpCmd(SUB_HEADING, "24. PCI Device ID"));
	helpList.push_back(helpCmd(HEADING, "--listamcversions           Show all AMC firmware versions"));
	helpList.push_back(
		helpCmd(HEADING, "-u,--username               Username used to authenticate for host redfish access"));
	helpList.push_back(
		helpCmd(HEADING, "-p,--password               Password used to authenticate for host redfish access"));
	helpList.push_back(helpCmd(
		HEADING, "-y,--assumeyes              Assume that the answer to any question which would be asked is yes"));

	printHelp(helpList, helpType);
	helpList.clear();
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
	vector<string> dumpArgs;
	string val = discCmds[discCmdType::DISC_DUMP].val;
	bool found = false;

	// Check if the dump command argument is valid
	if (val.empty()) {
		ERR("Dump command argument is required\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Dump command argument is a comma-separated list of numbers, with -1 being show all properties
	if (val == "-1") {
		// push all dump command types to the vector
		for (int i = 1; i < TOTAL_DISC_DUMPS; i++) {
			dumpArgs.push_back(to_string(i));
		}
	} else {
		// Split the dump command argument by commas
		stringstream ss(val.c_str());
		string token;
		while (getline(ss, token, ',')) {
			dumpArgs.push_back(token);
		}
	}

	// Check if the dump command argument is a number between 1 and TOTAL_DISC_DUMPS
	int dumpArg = atoi(val.c_str());
	if (dumpArg < DUMP_DEVICEID || dumpArg > TOTAL_DISC_DUMPS) {
		ERR("Invalid dump command argument. It must be between 1 and %d\n", TOTAL_DISC_DUMPS);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

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

	for (auto &arg : dumpArgs) {
		for (auto &cmd : dumpCmds) {
			if (cmd.type == atoi(arg.c_str())) {
				found = true;
				DBG("Running command: %d\n", cmd.type);
				result = (this->*cmd.func)(discCmds, d);
			}
		}
	}

	if (!found) {
		ERR("The following argument was not expected: '%s'.\n", val.c_str());
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	return result;
}

ze_result_t cmdDiscovery::deviceID(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	PRINT("  - Device ID: %d\n", d->index);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::deviceName(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	ze_device_properties_t zeDevProp;
	ze_result_t result;

	result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	PRINT("  - Device Name: %s\n", zeDevProp.name);

	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::vendorName(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	zes_device_properties_t zesDevProp = {};
	ze_result_t result;

	result = d->dev->zesGetDevProps(d->zesDeviceHdl, &zesDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	PRINT("  - Vendor Name: %s\n", zesDevProp.vendorName);

	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::socUuid(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	ze_device_properties_t devProp;
	ze_result_t result;

	result = d->dev->getDevProps(d->deviceHdl, &devProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	PRINT("  - SOC UUID: ");
	for (int j = 0; j < ZE_MAX_DEVICE_UUID_SIZE; ++j) {
		PRINT("%02X", devProp.uuid.id[j]);
	}
	PRINT("\n");

	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::serialNumber(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	zes_device_properties_t zesDevProp = {};
	ze_result_t result;

	result = d->dev->zesGetDevProps(d->zesDeviceHdl, &zesDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	PRINT("  - Serial Number: %s\n", zesDevProp.serialNumber);

	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::coreClockRate(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	ze_device_properties_t zeDevProp;
	ze_result_t result;

	result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	PRINT("  - Core clock rate: %d\n", zeDevProp.coreClockRate);

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
	zes_device_properties_t zesDevProp = {};
	ze_result_t result;

	result = d->dev->zesGetDevProps(d->zesDeviceHdl, &zesDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	PRINT("  - Driver Version: %s\n", zesDevProp.driverVersion);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::gfxFirmwareVersion(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	char version[MAX_PATH] = {0};
	UNUSED(discCmds);
	firmware *fw = (firmware *)d->dev->getFirmware();

	fw->getFWversion(fwType::GFX, version, sizeof(version));
	PRINT("  - GFX Firmware Version: %s\n", version);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::gfxDataFirmwareVersion(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	char version[MAX_PATH] = {0};
	UNUSED(discCmds);
	firmware *fw = (firmware *)d->dev->getFirmware();

	fw->getFWversion(fwType::GFX_DATA, version, sizeof(version));
	PRINT("  - GFX Data Firmware Version: %s\n", version);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::pciBDFAddress(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	zes_pci_properties_t pciProps;
	ze_result_t result;

	pci *p = (pci *)d->dev->getPCI();
	result = p->getProperties(d->zesDeviceHdl, &pciProps);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	PRINT("  - PCI BDF Address: %04x:%02x:%02x.%01x\n", pciProps.address.domain, pciProps.address.bus,
		  pciProps.address.device, pciProps.address.function);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::pciSlot(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);

	// Not implemented in XPUM for Windows. Linux version only.
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::pcieGeneration(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	zes_pci_properties_t pciProps;
	ze_result_t result;

	pci *p = (pci *)d->dev->getPCI();
	result = p->getProperties(d->zesDeviceHdl, &pciProps);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	PRINT("  - PCIe Generation: %d\n", pciProps.maxSpeed.gen);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::pcieMaxLinkWidth(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	zes_pci_properties_t pciProps;
	ze_result_t result;

	pci *p = (pci *)d->dev->getPCI();
	result = p->getProperties(d->zesDeviceHdl, &pciProps);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	PRINT("  - Max link width: %d\n", pciProps.maxSpeed.width);

	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::oamSocketID(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	UNUSED(d);
	// This was only implemented on PVC GPUs so should we simply return NA going forward?

	PRINT("  - OAM Socket ID: N/A\n");
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::memoryPhysicalSize(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	uint64_t physicalSize = 0;
	ze_result_t result;

	memory *m = (memory *)d->dev->getMemory();

	result = m->getMemorySize(&physicalSize);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory physical size: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	PRINT("  - Memory Physical Size: %" PRIu64 " MiB\n", physicalSize / 1024 / 1024);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::memoryChannels(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	uint32_t channels = 0;
	ze_result_t result;

	memory *m = (memory *)d->dev->getMemory();

	result = m->getMemoryChannels(&channels);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory channels: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	PRINT("  - Memory Channels: %d\n", channels);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::memoryBusWidth(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	uint32_t busWidth = 0;
	ze_result_t result;

	memory *m = (memory *)d->dev->getMemory();

	result = m->getMemoryBusWidth(&busWidth);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory bus width: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	PRINT("  - Memory Bus Width: %d bits\n", busWidth);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::eus(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	ze_device_properties_t zeDevProp;
	ze_result_t result;

	result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	PRINT("  - Number of EUs: %d\n",
		  zeDevProp.numEUsPerSubslice * zeDevProp.numSubslicesPerSlice * zeDevProp.numSlices);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::mediaEngines(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);

	enginegroup *eg = (enginegroup *)d->dev->getEngineGroup();
	if (eg == nullptr) {
		ERR("Failed to get engine group\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	uint32_t mediaEnginesCount = 0;
	ze_result_t result = eg->getMediaEngines(&mediaEnginesCount, ZES_ENGINE_GROUP_MEDIA_ALL);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get media engines count: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	PRINT("  - Number of Media Engines: %d\n", mediaEnginesCount);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::mediaEnhancementEngines(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	enginegroup *eg = (enginegroup *)d->dev->getEngineGroup();
	if (eg == nullptr) {
		ERR("Failed to get engine group\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	uint32_t mediaEnhancementEnginesCount = 0;
	ze_result_t result = eg->getMediaEngines(&mediaEnhancementEnginesCount, ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get media engines count: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	PRINT("  - Number of Media Enhancement Engines: %d\n", mediaEnhancementEnginesCount);
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
	ze_device_properties_t zeDevProp;
	ze_result_t result;

	result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	PRINT("  - Vendor ID: 0x%X\n", zeDevProp.vendorId);
	return ZE_RESULT_SUCCESS;
}

ze_result_t cmdDiscovery::pciDeviceID(discoveryCmdStruct *discCmds, devInfo *d)
{
	TRACING();
	UNUSED(discCmds);
	ze_device_properties_t zeDevProp = {};
	ze_result_t result;

	result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	PRINT("  - Device ID: 0x%X\n", zeDevProp.deviceId);
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
	vector<devInfo> deviceList;
	ze_result_t result;
	bool found = false;
	int opt;
	int optionIndex = 0;
	string shortOpts;
	vector<struct option> longOptsVec;

	processOptions(discCmds, ARRAY_SIZE(discCmds), shortOpts, longOptsVec);
	const struct option *longOpts = longOptsVec.data();
	// Skip the first two arguments (process and command name)
	int startind = 2;
	optind = 2;

	while ((opt = getopt_long(args->argc, args->argv, shortOpts.c_str(), longOpts, &optionIndex)) != -1) {
		switch (opt) {
		case 'h':
			help();
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
			for (auto &cmd : discCmds) {
				if (STRCASECMP(longOpts[optionIndex].name, cmd.opt.name) == 0) {
					discCmds[cmd.type].enabled = true;
					if (longOpts[optionIndex].has_arg == required_argument) {
						discCmds[cmd.type].val = optarg;
					}
					found = true;
					break;
				}
			}

			if (!found) {
				ERR("The following argument was not expected: '%s'.\n", longOpts[optionIndex].name);
				ERR("Run with --help for more information.\n");
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}

			break;
		default:
			ERR("The following argument was not expected: '%s'.\n", args->argv[startind]);
			ERR("Run with --help for more information.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		startind++;
	}

	// If optind is not equal to args->argc, it means there are extra arguments
	// that were not processed by getopt_long.
	if (optind != args->argc) {
		ERR("The following argument was not expected: '%s'.\n", args->argv[optind]);
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	result = args->sm.findDevice(discCmds[discCmdType::DISC_DEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '%s'.\n", discCmds[discCmdType::DISC_DEVICE].val.c_str());
		return result;
	}

	// If no args were provided, then we need to print out this info:
	//| 0   | Device Name: Intel(R) Graphics [0xe216]                                              |
	//      | Vendor Name: Intel(R) Corporation                                                    |
	//      | SOC UUID: 00000000-0000-0003-0000-0000e2168086                                       |
	//      | PCI BDF Address: 0000:03:00.0                                                        |
	//      | DRM Device: /dev/dri/card1                                                           |
	//      | Function Type: physical                                                              |
	if (args->argc == 2) {
		// Print out the device information
		for (auto &device : deviceList) {
			deviceName(discCmds, &device);
			vendorName(discCmds, &device);
			socUuid(discCmds, &device);
			pciBDFAddress(discCmds, &device);
			PRINT("==============================================\n");
		}
	} else {
		// Iterate through the device list and execute the command
		for (auto &device : deviceList) {
			// Call the appropriate command function based on the command type
			for (auto &cmd : discCmds) {
				if (cmd.enabled && cmd.func != nullptr) {
					DBG("Running command: %s\n", cmd.opt.name);
					(this->*cmd.func)(discCmds, &device);
				}
			}
		}
	}
	return 0;
}