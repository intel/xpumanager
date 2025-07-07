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
#include <iomanip>
#include <format>

/*
 * @brief This structure serves two purposes:
 * 1. It defines the command parsing for discovery commands.
 * 2. It allows for easy addition of new commands by simply adding a new entry to the array.
 * For example: The dump command requires a function of its own, so it is defined in the
 * discoveryCmdStruct with a pointer to the function that will be called when the command is executed
 */
discoveryCmdStruct discCmds[] = {
	{discCmdType::DISC_HELP, {"help", no_argument, 0, 'h'}},
	{discCmdType::DISC_JSON, {"json", no_argument, 0, 'j'}},
	{discCmdType::DISC_DEVICE, {"device", required_argument, 0, 'd'}},
	{discCmdType::DISC_PHYSICALFUNCTION, {"physicalFunction", no_argument, 0, 0}, &cmdDiscovery::physicalFunction},
	{discCmdType::DISC_VIRTUALFUNCTION, {"virtualFunction", no_argument, 0, 0}, &cmdDiscovery::virtualFunction},
	{discCmdType::DISC_DUMP, {"dump", required_argument, 0, 0}, &cmdDiscovery::dump, &cmdDiscovery::dumpHeading},
	{discCmdType::DISC_LISTAMCVERSIONS, {"listamcversions", no_argument, 0, 0}, &cmdDiscovery::listamcversions},
	{discCmdType::DISC_USERNAME, {"username", required_argument, 0, 'u'}},
	{discCmdType::DISC_PASSWORD, {"password", required_argument, 0, 'p'}},
	{discCmdType::DISC_ASSUMEYES, {"assumeyes", no_argument, 0, 'y'}},
};

discoveryDumpStruct discDumpCmds[] = {
	{DUMP_DEVICEID, &cmdDiscovery::deviceID, "Device ID"},
	{DUMP_DEVICENAME, &cmdDiscovery::deviceName, "Device Name"},
	{DUMP_VENDORNAME, &cmdDiscovery::vendorName, "Vendor Name"},
	{DUMP_SOCUUID, &cmdDiscovery::socUuid, "SOC UUID"},
	{DUMP_SERIALNUMBER, &cmdDiscovery::serialNumber, "Serial Number"},
	{DUMP_CORECLOCKRATE, &cmdDiscovery::coreClockRate, "Core Clock Rate"},
	{DUMP_STEPPING, &cmdDiscovery::stepping, "Stepping"},
	{DUMP_DRIVERVERSION, &cmdDiscovery::driverVersion, "Driver Version"},
	{DUMP_GFXFIRMWAREVERSION, &cmdDiscovery::gfxFirmwareVersion, "GFX Firmware Version"},
	{DUMP_GFXDATAFIRMWAREVERSION, &cmdDiscovery::gfxDataFirmwareVersion, "GFX Data Firmware Version"},
	{DUMP_PCIBDFADDRESS, &cmdDiscovery::pciBDFAddress, "PCI BDF Address"},
	{DUMP_PCISLOT, &cmdDiscovery::pciSlot, "PCI Slot"},
	{DUMP_PCIEGENERATION, &cmdDiscovery::pcieGeneration, "PCIe Generation"},
	{DUMP_PCIEMAXLINKWIDTH, &cmdDiscovery::pcieMaxLinkWidth, "PCIe Max Link Width"},
	{DUMP_OAMSOCID, &cmdDiscovery::oamSocketID, "OAM Socket ID"},
	{DUMP_MEMORYPHYSICALSIZE, &cmdDiscovery::memoryPhysicalSize, "Memory Physical Size"},
	{DUMP_MEMORYCHANNELS, &cmdDiscovery::memoryChannels, "Number of Memory Channels"},
	{DUMP_MEMORYBUSWIDTH, &cmdDiscovery::memoryBusWidth, "Memory Bus Width"},
	{DUMP_EUS, &cmdDiscovery::eus, "Number of EUs"},
	{DUMP_MEDIAENGINES, &cmdDiscovery::mediaEngines, "Number of Media Engines"},
	{DUMP_MEDIAENHANCEMENTENGINES, &cmdDiscovery::mediaEnhancementEngines, "Number of Media Enhancement Engines"},
	{DUMP_GFXFIRMWARESTATUS, &cmdDiscovery::gfxFirmwareStatus, "GFX Firmware Status"},
	{DUMP_PCIVENDORID, &cmdDiscovery::pciVendorID, "PCI Vendor ID"},
	{DUMP_PCIDEVICEID, &cmdDiscovery::pciDeviceID, "PCI Device ID"},
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

/**
 * @brief This function does the common pre-checks for the discovery commands.
 *
 * @param d A pointer to the device info structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::preCheck(vector<string> *dumpArgs)
{
	string val = discCmds[discCmdType::DISC_DUMP].val;

	// Check if the dump command argument is valid
	if (val.empty()) {
		ERR("Dump command argument is required\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Dump command argument is a comma-separated list of numbers, with -1 being show all properties
	if (val == "-1") {
		// push all dump command types to the vector
		for (int i = 1; i < TOTAL_DISC_DUMPS; i++) {
			dumpArgs->push_back(to_string(i));
		}
	} else {
		// Split the dump command argument by commas
		stringstream ss(val.c_str());
		string token;
		while (getline(ss, token, ',')) {
			dumpArgs->push_back(token);
		}
	}

	// Check if the dump command argument is a number between 1 and TOTAL_DISC_DUMPS
	int dumpArg = atoi(val.c_str());
	if (dumpArg < DUMP_DEVICEID || dumpArg > TOTAL_DISC_DUMPS) {
		ERR("Invalid dump command argument. It must be between 1 and %d\n", TOTAL_DISC_DUMPS);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the dump heading command. This command print the heading
 *
 * @param d A pointer to the device info structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::dumpHeading()
{
	TRACING();
	vector<string> dumpArgs;
	ze_result_t result;
	bool found = false;
	string val = discCmds[discCmdType::DISC_DUMP].val;

	result = preCheck(&dumpArgs);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	for (auto &arg : dumpArgs) {
		for (auto &cmd : discDumpCmds) {
			if (cmd.type == atoi(arg.c_str())) {
				// found will only be true if we have printed at least one heading, so add a comma before the next
				// heading
				if (found) {
					PRINT(", ");
				}
				PRINT("%s", cmd.heading.c_str());
				found = true;
			}
		}
	}

	if (!found) {
		ERR("The following argument was not expected: '%s'.\n", val.c_str());
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	} else {
		// Print a new line after the heading
		PRINT("\n");
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the dump command. This command dumps the device properties
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::dump(devInfo *d, string *outputLine)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	vector<string> dumpArgs;
	string val = discCmds[discCmdType::DISC_DUMP].val;
	bool found = false;

	result = preCheck(&dumpArgs);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	for (auto &arg : dumpArgs) {
		for (auto &cmd : discDumpCmds) {
			if (cmd.type == atoi(arg.c_str())) {
				DBG("Running command: %d\n", cmd.type);
				result = (this->*cmd.func)(d, outputLine);
				if (result != ZE_RESULT_SUCCESS) {
					return result;
				}
				// found will only be true if we have printed at least one output, so add a comma before the next
				// output
				if (found) {
					PRINT(", ");
				}
				PRINT("%s", outputLine->c_str());
				// Clear the output line for the next command
				outputLine->clear();
				// Set found to true to indicate that we have printed at least one command
				found = true;
				break;
			}
		}
	}

	if (!found) {
		ERR("The following argument was not expected: '%s'.\n", val.c_str());
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	} else {
		// Print a new line after the output
		PRINT("\n");
	}

	return result;
}

/**
 * @brief Prints out the device ID of the device when user runs discovery --dump 1
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::deviceID(devInfo *d, string *outputLine)
{
	TRACING();

	*outputLine = to_string(d->index);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Print out the device name for a device when user runs discovery --dump 2.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::deviceName(devInfo *d, string *outputLine)
{
	TRACING();

	ze_device_properties_t zeDevProp = {};
	ze_result_t result;

	result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	*outputLine = zeDevProp.name;

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints out the vendor name of a device when user runs discovery --dump 3.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::vendorName(devInfo *d, string *outputLine)
{
	TRACING();

	zes_device_properties_t zesDevProp = {};
	ze_result_t result;

	result = d->dev->zesGetDevProps(d->zesDeviceHdl, &zesDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = zesDevProp.vendorName;

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Print the SOC UUID command for a device when user runs discovery --dump 4.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::socUuid(devInfo *d, string *outputLine)
{
	TRACING();

	ze_device_properties_t devProp = {};
	ze_result_t result;
	char output[256] = {0};

	result = d->dev->getDevProps(d->deviceHdl, &devProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	snprintf(output, sizeof(output), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			 devProp.uuid.id[15], devProp.uuid.id[14], devProp.uuid.id[13], devProp.uuid.id[12], devProp.uuid.id[11],
			 devProp.uuid.id[10], devProp.uuid.id[9], devProp.uuid.id[8], devProp.uuid.id[7], devProp.uuid.id[6],
			 devProp.uuid.id[5], devProp.uuid.id[4], devProp.uuid.id[3], devProp.uuid.id[2], devProp.uuid.id[1],
			 devProp.uuid.id[0]);

	*outputLine = output;

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the serial number for a device when user runs discovery --dump 5
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::serialNumber(devInfo *d, string *outputLine)
{
	TRACING();

	zes_device_properties_t zesDevProp = {};
	ze_result_t result;

	result = d->dev->zesGetDevProps(d->zesDeviceHdl, &zesDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = zesDevProp.serialNumber;

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the core clock rate for a device when user runs discovery --dump 6
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::coreClockRate(devInfo *d, string *outputLine)
{
	TRACING();

	ze_device_properties_t zeDevProp = {};
	ze_result_t result;

	result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = to_string(zeDevProp.coreClockRate) + " MHz";

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the stepping for a device when user runs discovery --dump 7.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::stepping(devInfo *d, string *outputLine)
{
	TRACING();

	UNUSED(d);
	UNUSED(outputLine);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the driver version for a device when user runs discovery --dump 8.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::driverVersion(devInfo *d, string *outputLine)
{
	TRACING();

	zes_device_properties_t zesDevProp = {};
	ze_result_t result;

	result = d->dev->zesGetDevProps(d->zesDeviceHdl, &zesDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = zesDevProp.driverVersion;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the GFX firmware version for a device when user runs discovery --dump 9.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::gfxFirmwareVersion(devInfo *d, string *outputLine)
{
	TRACING();
	char version[MAX_PATH] = {0};

	firmware *fw = (firmware *)d->dev->getFirmware();

	fw->getFWversion(fwType::GFX, version, sizeof(version));
	*outputLine = version;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the GFX data firmware version for a device when user runs discovery --dump 10.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::gfxDataFirmwareVersion(devInfo *d, string *outputLine)
{
	TRACING();
	char version[MAX_PATH] = {0};

	firmware *fw = (firmware *)d->dev->getFirmware();

	fw->getFWversion(fwType::GFX_DATA, version, sizeof(version));
	*outputLine = version;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the PCI BDF address for a device when user runs discovery --dump 11.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::pciBDFAddress(devInfo *d, string *outputLine)
{
	TRACING();

	zes_pci_properties_t pciProps;
	ze_result_t result;
	char output[256] = {0};

	pci *p = (pci *)d->dev->getPCI();
	result = p->getProperties(d->zesDeviceHdl, &pciProps);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	// Format the PCI BDF address as "domain:bus:device.function"
	// Example: "0000:00:02.0"
	// where domain is 4 digits, bus is 2 digits in hex, device is 2 digits, and function is 1 digit.
	snprintf(output, 255, "%04x:%02x:%02x.%01x", pciProps.address.domain, pciProps.address.bus, pciProps.address.device,
			 pciProps.address.function);
	*outputLine = output;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the PCI slot for a device when user runs discovery --dump 12.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::pciSlot(devInfo *d, string *outputLine)
{
	TRACING();

	UNUSED(d);

	// Not implemented in XPUM for Windows. Linux version only.
	*outputLine = "";
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the PCIe generation for a device when user runs discovery --dump 13.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::pcieGeneration(devInfo *d, string *outputLine)
{
	TRACING();

	zes_pci_properties_t pciProps;
	ze_result_t result;

	pci *p = (pci *)d->dev->getPCI();
	result = p->getProperties(d->zesDeviceHdl, &pciProps);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = to_string(pciProps.maxSpeed.gen);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the PCIe max link width for a device when user runs discovery --dump 14.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::pcieMaxLinkWidth(devInfo *d, string *outputLine)
{
	TRACING();

	zes_pci_properties_t pciProps;
	ze_result_t result;

	pci *p = (pci *)d->dev->getPCI();
	result = p->getProperties(d->zesDeviceHdl, &pciProps);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	*outputLine = to_string(pciProps.maxSpeed.width);

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the OAM socket ID for a device when user runs discovery --dump 15.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::oamSocketID(devInfo *d, string *outputLine)
{
	TRACING();

	UNUSED(d);
	// This was only implemented on PVC GPUs so should we simply return NA going forward?

	*outputLine = "N/A";
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the memory physical size for a device when user runs discovery --dump 16.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::memoryPhysicalSize(devInfo *d, string *outputLine)
{
	TRACING();

	uint64_t physicalSize = 0;
	ze_result_t result;

	memory *m = (memory *)d->dev->getMemory();

	result = m->getMemorySize(&physicalSize);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory physical size: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	stringstream stream;
	stream << fixed << setprecision(2) << (double)physicalSize / 1024 / 1024;
	*outputLine = stream.str() + " MiB";
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the memory channels for a device when user runs discovery --dump 17.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::memoryChannels(devInfo *d, string *outputLine)
{
	TRACING();

	uint32_t channels = 0;
	ze_result_t result;

	memory *m = (memory *)d->dev->getMemory();

	result = m->getMemoryChannels(&channels);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory channels: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = to_string(channels);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the memory bus width for a device when user runs discovery --dump 18.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::memoryBusWidth(devInfo *d, string *outputLine)
{
	TRACING();

	uint32_t busWidth = 0;
	ze_result_t result;

	memory *m = (memory *)d->dev->getMemory();

	result = m->getMemoryBusWidth(&busWidth);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory bus width: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = to_string(busWidth);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the EUs for a device when user runs discovery --dump 19.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::eus(devInfo *d, string *outputLine)
{
	TRACING();

	ze_device_properties_t zeDevProp = {};
	ze_result_t result;

	zeDevProp.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
	ze_eu_count_ext_t *extendedPropertiesPtr = new ze_eu_count_ext_t();
	memset(extendedPropertiesPtr, 0, sizeof(ze_eu_count_ext_t));
	extendedPropertiesPtr->stype = ZE_STRUCTURE_TYPE_EU_COUNT_EXT;
	zeDevProp.pNext = extendedPropertiesPtr;

	result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		delete extendedPropertiesPtr;
		return result;
	}

	if (zeDevProp.pNext != nullptr) {
		ze_eu_count_ext_t *eu_count_ext = (ze_eu_count_ext_t *)(zeDevProp.pNext);
		*outputLine = to_string(eu_count_ext->numTotalEUs);
	}

	delete extendedPropertiesPtr;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the media engines for a device when user runs discovery --dump 20.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::mediaEngines(devInfo *d, string *outputLine)
{
	TRACING();

	enginegroup *eg = (enginegroup *)d->dev->getEngineGroup();
	if (eg == nullptr) {
		ERR("Failed to get engine group\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	uint32_t mediaEnginesCount = 0;
	ze_result_t result = eg->getMediaEngines(&mediaEnginesCount, ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get media engines count: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = to_string(mediaEnginesCount);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the media enhancement engines for a device when user runs discovery --dump 21.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::mediaEnhancementEngines(devInfo *d, string *outputLine)
{
	TRACING();

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

	*outputLine = to_string(mediaEnhancementEnginesCount);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the GFX firmware status for a device when user runs discovery --dump 22.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::gfxFirmwareStatus(devInfo *d, string *outputLine)
{
	TRACING();

	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the PCI vendor ID for a device when user runs discovery --dump 23.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::pciVendorID(devInfo *d, string *outputLine)
{
	TRACING();

	ze_device_properties_t zeDevProp = {};
	ze_result_t result;

	result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	stringstream stream;
	stream << hex << zeDevProp.vendorId;
	string hexString = stream.str();
	*outputLine = "0x" + hexString;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the PCI device ID for a device when user runs discovery --dump 24.
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::pciDeviceID(devInfo *d, string *outputLine)
{
	TRACING();

	ze_device_properties_t zeDevProp = {};
	ze_result_t result;

	result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	stringstream stream;
	stream << hex << zeDevProp.deviceId;
	string hexString = stream.str();
	*outputLine = "0x" + hexString;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the physical function for a device
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::physicalFunction(devInfo *d, string *outputLine)
{
	TRACING();

	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the virtual function for a device
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::virtualFunction(devInfo *d, string *outputLine)
{
	TRACING();

	UNUSED(d);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief  Lists the AMC versions for a device
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::listamcversions(devInfo *d, string *outputLine)
{
	TRACING();

	UNUSED(d);
	UNUSED(outputLine);
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
	bool found = false, headingFirst = true;
	int opt;
	int optionIndex = 0;
	string shortOpts;
	vector<struct option> longOptsVec;
	string outputLine = "";

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
			deviceName(&device, &outputLine);
			PRINT("deviceID: %s\n", outputLine.c_str());
			outputLine.clear();
			vendorName(&device, &outputLine);
			PRINT("vendorName: %s\n", outputLine.c_str());
			outputLine.clear();
			socUuid(&device, &outputLine);
			PRINT("socUuid: %s\n", outputLine.c_str());
			outputLine.clear();
			pciBDFAddress(&device, &outputLine);
			PRINT("pciBDFAddress: %s\n", outputLine.c_str());
			PRINT("==============================================\n");
		}
	} else {
		// Iterate through the device list and execute the command
		for (auto &device : deviceList) {
			outputLine.clear();

			// Call the appropriate command function based on the command type
			for (auto &cmd : discCmds) {
				if (cmd.enabled && cmd.func != nullptr) {
					// If there is a heading function, call it first
					if (cmd.headingFunc != nullptr && headingFirst) {
						headingFirst = false;
						result = (this->*cmd.headingFunc)();
						if (result != ZE_RESULT_SUCCESS) {
							return result;
						}
					}

					DBG("Running command: %s\n", cmd.opt.name);
					result = (this->*cmd.func)(&device, &outputLine);
					if (result != ZE_RESULT_SUCCESS) {
						return result;
					}
				}
			}
		}
	}
	return 0;
}
