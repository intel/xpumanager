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
#include "printer.h"
#include <assert.h>
#include <enginegroup.h>
#include <firmware.h>
#include <memory.h>
#include <pci.h>
#include <sstream>
#include <iomanip>
#include <format>

/**
 * @brief This structure serves two purposes:
 * 1. It defines the command parsing for discovery commands.
 * 2. It allows for easy addition of new commands by simply adding a new entry to the map.
 * For example: The dump command requires a function of its own, so it is defined in the
 * discoveryCmdStruct with a pointer to the function that will be called when the command is executed
 */
static std::unordered_map<discCmdType, discoveryCmdStruct> discCmds = {
	{discCmdType::DISC_HELP, {{"help", no_argument, 0, 'h'}, nullptr, nullptr, false, ""}},
	{discCmdType::DISC_JSON, {{"json", no_argument, 0, 'j'}, nullptr, nullptr, false, ""}},
	{discCmdType::DISC_DEVICE, {{"device", required_argument, 0, 'd'}, &cmdDiscovery::dumpAll, nullptr, false, ""}},
	{discCmdType::DISC_PF, {{"pf", no_argument, 0, 0}, nullptr, nullptr, false, ""}},
	{discCmdType::DISC_PHYSICALFUNCTION, {{"physicalFunction", no_argument, 0, 0}, nullptr, nullptr, false, ""}},
	{discCmdType::DISC_VF, {{"vf", no_argument, 0, 0}, nullptr, nullptr, false, ""}},
	{discCmdType::DISC_VIRTUALFUNCTION, {{"virtualFunction", no_argument, 0, 0}, nullptr, nullptr, false, ""}},
	{discCmdType::DISC_DUMP,
	 {{"dump", required_argument, 0, 0}, &cmdDiscovery::dump, &cmdDiscovery::dumpHeading, false, ""}},
	{discCmdType::DISC_LISTAMCVERSIONS,
	 {{"listamcversions", no_argument, 0, 0}, &cmdDiscovery::listamcversions, nullptr, false, ""}},
	{discCmdType::DISC_USERNAME, {{"username", required_argument, 0, 'u'}, nullptr, nullptr, false, ""}},
	{discCmdType::DISC_PASSWORD, {{"password", required_argument, 0, 'p'}, nullptr, nullptr, false, ""}},
	{discCmdType::DISC_ASSUMEYES, {{"assumeyes", no_argument, 0, 'y'}, nullptr, nullptr, false, ""}},
};

static std::unordered_map<int, discoveryDumpStruct> discDumpCmds = {
	{DUMP_DEVICEID, {&cmdDiscovery::deviceID, "Device ID"}},
	{DUMP_DEVICENAME, {&cmdDiscovery::deviceName, "Device Name"}},
	{DUMP_VENDORNAME, {&cmdDiscovery::vendorName, "Vendor Name"}},
	{DUMP_SOCUUID, {&cmdDiscovery::socUuid, "SOC UUID"}},
	{DUMP_SERIALNUMBER, {&cmdDiscovery::serialNumber, "Serial Number"}},
	{DUMP_CORECLOCKRATE, {&cmdDiscovery::coreClockRate, "Core Clock Rate"}},
	{DUMP_STEPPING, {&cmdDiscovery::stepping, "Stepping"}},
	{DUMP_DRIVERVERSION, {&cmdDiscovery::driverVersion, "Driver Version"}},
	{DUMP_GFXFIRMWAREVERSION, {&cmdDiscovery::gfxFirmwareVersion, "GFX Firmware Version"}},
	{DUMP_GFXDATAFIRMWAREVERSION, {&cmdDiscovery::gfxDataFirmwareVersion, "GFX Data Firmware Version"}},
	{DUMP_PCIBDFADDRESS, {&cmdDiscovery::pciBDFAddress, "PCI BDF Address"}},
	{DUMP_PCISLOT, {&cmdDiscovery::pciSlot, "PCI Slot"}},
	{DUMP_PCIEGENERATION, {&cmdDiscovery::pcieGeneration, "PCIe Generation"}},
	{DUMP_PCIEMAXLINKWIDTH, {&cmdDiscovery::pcieMaxLinkWidth, "PCIe Max Link Width"}},
	{DUMP_OAMSOCID, {&cmdDiscovery::oamSocketID, "OAM Socket ID"}},
	{DUMP_MEMORYPHYSICALSIZE, {&cmdDiscovery::memoryPhysicalSize, "Memory Physical Size"}},
	{DUMP_MEMORYCHANNELS, {&cmdDiscovery::memoryChannels, "Number of Memory Channels"}},
	{DUMP_MEMORYBUSWIDTH, {&cmdDiscovery::memoryBusWidth, "Memory Bus Width"}},
	{DUMP_EUS, {&cmdDiscovery::eus, "Number of EUs"}},
	{DUMP_MEDIAENGINES, {&cmdDiscovery::mediaEngines, "Number of Media Engines"}},
	{DUMP_MEDIAENHANCEMENTENGINES, {&cmdDiscovery::mediaEnhancementEngines, "Number of Media Enhancement Engines"}},
	{DUMP_GFXFIRMWARESTATUS, {&cmdDiscovery::gfxFirmwareStatus, "GFX Firmware Status"}},
	{DUMP_PCIVENDORID, {&cmdDiscovery::pciVendorID, "PCI Vendor ID"}},
	{DUMP_PCIDEVICEID, {&cmdDiscovery::pciDeviceID, "PCI Device ID"}},
};

/**
 * @brief Helper function to convert internal JSON keys to user-friendly display names
 *
 * @param key The internal JSON key
 * @return std::string The user-friendly display name
 */
std::string getDisplayName(const std::string &key)
{
	static const std::unordered_map<std::string, std::string> keyDisplayMap = {
		// clang-format off
		{"device_id", "Device ID"},
		{"device_name", "Device Name"},
		{"vendor_name", "Vendor Name"},
		{"soc_uuid", "SOC UUID"},
		{"pci_bdf_address", "PCI BDF Address"},
		{"drm_device_path", "DRM Device"},
		{"function_type", "Function Type"},
		// clang-format on
	};

	auto it = keyDisplayMap.find(key);
	if (it != keyDisplayMap.end()) {
		return it->second;
	}
	// If key not found in map, return the original key (fallback)
	return key;
}

/**
 * @brief Constructor for DiscoveryTextPrinter class
 */
DiscoveryTextPrinter::DiscoveryTextPrinter() : TextPrinter() {}

/**
 * @brief Prints text output with custom formatting for discovery command
 *
 * @param jsonObj Pointer to the JSON object to be formatted and printed as text
 */
void DiscoveryTextPrinter::print(nlohmann::ordered_json *jsonObj)
{
	// Custom formatting for discovery text output
	if (jsonObj->contains("heading")) {
		// Print CSV-style headers for dump command
		const auto &headers = (*jsonObj)["heading"];
		for (size_t i = 0; i < headers.size(); ++i) {
			PRINT("%s", headers[i].get<std::string>().c_str());
			if (i < headers.size() - 1) {
				PRINT(", ");
			}
		}
		PRINT("\n");

		// Print values for each device
		for (auto &deviceItem : jsonObj->items()) {
			if (deviceItem.key() != "heading") {
				const auto &values = deviceItem.value();
				for (size_t i = 0; i < values.size(); ++i) {
					PRINT("%s", values[i].get<std::string>().c_str());
					if (i < values.size() - 1) {
						PRINT(", ");
					}
				}
				PRINT("\n");
			}
		}
	} else if (jsonObj->contains("device_list")) {
		// Handle JSON object with device_list field - print device information in a formatted table-like structure
		for (auto &device : (*jsonObj)["device_list"]) {
			bool firstItem = true;
			for (auto &item : device.items()) {
				if (firstItem) {
					// For the first item, print it as the main device identifier
					std::string deviceLine = "| " + item.value().get<std::string>();
					PRINT("%-70s|\n", deviceLine.c_str());

					firstItem = false;
				} else {
					// For subsequent items, print them indented with display names
					std::string displayKey = getDisplayName(item.key());
					std::string detailLine = "      | " + displayKey + ": " + item.value().get<std::string>();
					PRINT("%-70s|\n", detailLine.c_str());
				}
			}
			PRINT("\n");
		}
	} else {
		// Handle single device or key-value pairs
		for (auto &item : jsonObj->items()) {
			std::string displayKey = getDisplayName(item.key());
			std::string detailLine = "| " + displayKey + ": " + item.value().get<std::string>();
			PRINT("%-70s |\n", detailLine.c_str());
		}
	}
}

/**
 * @brief Prints detailed device information in JSON format
 *
 * @param device Pointer to the device info structure
 * @param funcType Function type (physical or virtual)
 * @return std::unique_ptr<nlohmann::ordered_json> JSON object containing device details
 */
std::unique_ptr<nlohmann::ordered_json> cmdDiscovery::printDeviceDetail(devInfo *device, devFuncType funcType)
{
	auto jsonObj = std::make_unique<nlohmann::ordered_json>();
	std::string outputLine;

	// Get string values first, then assign to JSON (simple format for JSON output)
	(*jsonObj)["device_id"] = std::to_string(device->index);
	deviceName(device, &outputLine);
	(*jsonObj)["device_name"] = outputLine;

	vendorName(device, &outputLine);
	(*jsonObj)["vendor_name"] = outputLine;

	socUuid(device, &outputLine);
	(*jsonObj)["soc_uuid"] = outputLine;

	pciBDFAddress(device, &outputLine);
	(*jsonObj)["pci_bdf_address"] = outputLine;

	(*jsonObj)["drm_device_path"] = device->dev->getDrmDevPath();

	(*jsonObj)["function_type"] = (funcType == DEVICE_FUNCTION_TYPE_PHYSICAL) ? "physical" : "virtual";

	return jsonObj;
}

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdDiscovery::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

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
ze_result_t cmdDiscovery::preCheck(std::vector<std::string> *dumpArgs)
{
	std::string val = discCmds[discCmdType::DISC_DUMP].val;

	// Check if the dump command argument is valid
	if (val.empty()) {
		ERR("Dump command argument is required\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Dump command argument is a comma-separated list of numbers, with -1 being show all properties
	if (val == "-1") {
		// push all dump command types to the vector
		for (int i = 1; i < TOTAL_DISC_DUMPS; i++) {
			dumpArgs->push_back(std::to_string(i));
		}
	} else {
		// Split the dump command argument by commas
		std::stringstream ss(val.c_str());
		std::string token;
		while (getline(ss, token, ',')) {
			dumpArgs->push_back(token);
		}
	}

	// Check if each dump command argument is a number between 1 and TOTAL_DISC_DUMPS
	for (const auto &arg : *dumpArgs) {
		int dumpArg = atoi(arg.c_str());
		if (dumpArg < DUMP_DEVICEID || dumpArg > TOTAL_DISC_DUMPS) {
			ERR("Invalid dump command argument '%s'. It must be between 1 and %d\n", arg.c_str(), TOTAL_DISC_DUMPS);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
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
ze_result_t cmdDiscovery::dumpHeading(nlohmann::ordered_json *jsonObj)
{
	TRACING();
	std::vector<std::string> dumpArgs;
	ze_result_t result;
	bool found = false;
	std::string val = discCmds[discCmdType::DISC_DUMP].val;
	auto headingJson = std::make_unique<nlohmann::ordered_json>();

	result = preCheck(&dumpArgs);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	for (const auto &arg : dumpArgs) {
		for (const auto &cmd : discDumpCmds) {
			if (cmd.first == stoi(arg)) {
				found = true;
				headingJson->push_back(cmd.second.heading);
			}
		}
	}

	if (!found) {
		ERR("The following argument was not expected: '%s'.\n", val.c_str());
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	(*jsonObj)["heading"] = *headingJson;
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
ze_result_t cmdDiscovery::dump(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	std::vector<std::string> dumpArgs;
	std::string val = discCmds[discCmdType::DISC_DUMP].val;
	auto valuesJson = std::make_unique<nlohmann::ordered_json>();
	bool found = false;

	result = preCheck(&dumpArgs);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	std::string outputLine;
	for (const auto &arg : dumpArgs) {
		for (const auto &cmd : discDumpCmds) {
			if (cmd.first == stoi(arg)) {
				DBG("Running command: %d\n", cmd.first);
				result = (this->*cmd.second.func)(d, &outputLine);
				if (result != ZE_RESULT_SUCCESS) {
					return result;
				}
				valuesJson->push_back(outputLine);
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
	}
	(*jsonObj)[std::to_string(d->index)] = *valuesJson;
	return result;
}

/**
 * @brief Executes the dumpAll command. This command dumps all the device properties
 *
 * @param d A pointer to the device info structure.
 * @param outputLine A pointer to the output line string.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::dumpAll(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();
	std::string outputLine;
	ze_result_t result = ZE_RESULT_SUCCESS;

	// We should dump all properties for a device only when no other command line options are specified.
	for (const auto &cmd : discCmds) {
		if (cmd.second.enabled && (cmd.first != discCmdType::DISC_DEVICE && cmd.first != discCmdType::DISC_JSON)) {
			// Silently return if any other command line options are specified
			return result;
		}
	}

	// Iterate over all dump commands and execute them
	for (const auto &cmd : discDumpCmds) {
		result = (this->*cmd.second.func)(d, &outputLine);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
		(*jsonObj)[cmd.second.heading] = outputLine;
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
ze_result_t cmdDiscovery::deviceID(devInfo *d, std::string *outputLine)
{
	TRACING();

	*outputLine = std::to_string(d->index);
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
ze_result_t cmdDiscovery::deviceName(devInfo *d, std::string *outputLine)
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
ze_result_t cmdDiscovery::vendorName(devInfo *d, std::string *outputLine)
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
ze_result_t cmdDiscovery::socUuid(devInfo *d, std::string *outputLine)
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
ze_result_t cmdDiscovery::serialNumber(devInfo *d, std::string *outputLine)
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
ze_result_t cmdDiscovery::coreClockRate(devInfo *d, std::string *outputLine)
{
	TRACING();

	ze_device_properties_t zeDevProp = {};
	ze_result_t result;

	result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = std::to_string(zeDevProp.coreClockRate) + " MHz";

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
ze_result_t cmdDiscovery::stepping(UNUSED devInfo *d, UNUSED std::string *outputLine)
{
	TRACING();

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
ze_result_t cmdDiscovery::driverVersion(devInfo *d, std::string *outputLine)
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
ze_result_t cmdDiscovery::gfxFirmwareVersion(devInfo *d, std::string *outputLine)
{
	TRACING();
	char version[MAX_PATH] = {0};

	firmware *fw = d->dev->getFirmware();

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
ze_result_t cmdDiscovery::gfxDataFirmwareVersion(devInfo *d, std::string *outputLine)
{
	TRACING();
	char version[MAX_PATH] = {0};

	firmware *fw = d->dev->getFirmware();

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
ze_result_t cmdDiscovery::pciBDFAddress(devInfo *d, std::string *outputLine)
{
	TRACING();

	pci *p = d->dev->getPCI();
	if (p == nullptr) {
		ERR("Failed to get PCI device properties.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	*outputLine = p->getBDFStr();
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
ze_result_t cmdDiscovery::pciSlot(UNUSED devInfo *d, std::string *outputLine)
{
	TRACING();

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
ze_result_t cmdDiscovery::pcieGeneration(devInfo *d, std::string *outputLine)
{
	TRACING();

	zes_pci_properties_t pciProps;
	ze_result_t result;

	pci *p = d->dev->getPCI();
	result = p->getProperties(d->zesDeviceHdl, &pciProps);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = std::to_string(pciProps.maxSpeed.gen);
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
ze_result_t cmdDiscovery::pcieMaxLinkWidth(devInfo *d, std::string *outputLine)
{
	TRACING();

	zes_pci_properties_t pciProps;
	ze_result_t result;

	pci *p = d->dev->getPCI();
	result = p->getProperties(d->zesDeviceHdl, &pciProps);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	*outputLine = std::to_string(pciProps.maxSpeed.width);

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
ze_result_t cmdDiscovery::oamSocketID(UNUSED devInfo *d, std::string *outputLine)
{
	TRACING();

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
ze_result_t cmdDiscovery::memoryPhysicalSize(devInfo *d, std::string *outputLine)
{
	TRACING();

	uint64_t physicalSize = 0;
	ze_result_t result;

	memory *m = d->dev->getMemory();

	result = m->getMemorySize(&physicalSize);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory physical size: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	std::stringstream stream;
	stream << std::fixed << std::setprecision(2) << (double)physicalSize / 1024 / 1024;
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
ze_result_t cmdDiscovery::memoryChannels(devInfo *d, std::string *outputLine)
{
	TRACING();

	uint32_t channels = 0;
	ze_result_t result;

	memory *m = d->dev->getMemory();

	result = m->getMemoryChannels(&channels);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory channels: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = std::to_string(channels);
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
ze_result_t cmdDiscovery::memoryBusWidth(devInfo *d, std::string *outputLine)
{
	TRACING();

	uint32_t busWidth = 0;
	ze_result_t result;

	memory *m = d->dev->getMemory();

	result = m->getMemoryBusWidth(&busWidth);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory bus width: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = std::to_string(busWidth);
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
ze_result_t cmdDiscovery::eus(devInfo *d, std::string *outputLine)
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
		*outputLine = std::to_string(eu_count_ext->numTotalEUs);
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
ze_result_t cmdDiscovery::mediaEngines(devInfo *d, std::string *outputLine)
{
	TRACING();

	enginegroup *eg = d->dev->getEngineGroup();
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

	*outputLine = std::to_string(mediaEnginesCount);
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
ze_result_t cmdDiscovery::mediaEnhancementEngines(devInfo *d, std::string *outputLine)
{
	TRACING();

	enginegroup *eg = d->dev->getEngineGroup();
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

	*outputLine = std::to_string(mediaEnhancementEnginesCount);
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
ze_result_t cmdDiscovery::gfxFirmwareStatus(devInfo *d, std::string *outputLine)
{
	TRACING();

	pci *p = d->dev->getPCI();
	if (p == nullptr) {
		ERR("Failed to get PCI device properties.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	auto fwStatus = p->getFWStatus();
	*outputLine = fwStatus;
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
ze_result_t cmdDiscovery::pciVendorID(devInfo *d, std::string *outputLine)
{
	TRACING();

	ze_device_properties_t zeDevProp = {};
	ze_result_t result;

	result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	std::stringstream stream;
	stream << std::hex << zeDevProp.vendorId;
	std::string hexString = stream.str();
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
ze_result_t cmdDiscovery::pciDeviceID(devInfo *d, std::string *outputLine)
{
	TRACING();

	ze_device_properties_t zeDevProp = {};
	ze_result_t result;

	result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	std::stringstream stream;
	stream << std::hex << zeDevProp.deviceId;
	std::string hexString = stream.str();
	*outputLine = "0x" + hexString;
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
ze_result_t cmdDiscovery::listamcversions(UNUSED devInfo *d, UNUSED nlohmann::ordered_json *Output)
{
	TRACING();

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief  Gets the function type of a device
 *
 * @param d A pointer to the device info structure.
 *
 * @return devFuncType Returns the function type of the device.
 */
devFuncType cmdDiscovery::getFuncType(devInfo *d)
{
	TRACING();
	zes_pci_properties_t pciProps;
	ze_result_t result;

	if (d == nullptr) {
		ERR("Invalid device info structure.\n");
		return DEVICE_FUNCTION_TYPE_UNKNOWN;
	}

	pci *p = d->dev->getPCI();
	result = p->getProperties(d->zesDeviceHdl, &pciProps);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return DEVICE_FUNCTION_TYPE_UNKNOWN;
	}

	return (pciProps.address.function == 0) ? DEVICE_FUNCTION_TYPE_PHYSICAL : DEVICE_FUNCTION_TYPE_VIRTUAL;
}

/**
 * @brief Prints device information.
 *
 * @param deviceList A list of device information structures.
 * @param type The type of device function to filter by.
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::printDeviceInfo(std::vector<devInfo> deviceList, std::unique_ptr<Printer> &printer,
										  devFuncType type)
{
	TRACING();
	std::string outputLine = "";
	bool found = false;
	devFuncType foundType;

	auto deviceListJson = std::make_unique<nlohmann::ordered_json>();

	for (auto &device : deviceList) {
		foundType = getFuncType(&device);
		if (type != DEVICE_FUNCTION_TYPE_ALL && foundType != type) {
			continue; // Skip devices that do not match the specified function type
		}

		found = true;
		auto deviceJson = printDeviceDetail(&device, foundType);
		deviceListJson->push_back(*deviceJson);
	}

	auto devicesJson = std::make_unique<nlohmann::ordered_json>();
	if (!found) {
		// Check if JSON output is enabled
		if (discCmds[discCmdType::DISC_JSON].enabled) {
			(*devicesJson)["device_list"] = nullptr;
		} else {
			(*devicesJson)["device_list"] = "No device discovered";
		}
	} else {
		(*devicesJson)["device_list"] = *deviceListJson;
	}
	printer->print(devicesJson.get());

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
	std::vector<devInfo> deviceList;
	ze_result_t result;
	bool found = false, headingFirst = true;
	int opt;
	int optionIndex = 0;
	std::unique_ptr<Printer> printer;
	std::string shortOpts;
	std::vector<struct option> longOptsVec;

	processOptions(discCmds, shortOpts, longOptsVec);
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
				if (STRCASECMP(longOpts[optionIndex].name, cmd.second.opt.name) == 0) {
					cmd.second.enabled = true;
					if (longOpts[optionIndex].has_arg == required_argument) {
						cmd.second.val = optarg;
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
	if (discCmds[discCmdType::DISC_JSON].enabled == true) {
		printer = std::make_unique<JsonPrinter>();
	} else {
		printer = std::make_unique<DiscoveryTextPrinter>();
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
	if (args->argc == 2 || (args->argc == 3 && discCmds[discCmdType::DISC_JSON].enabled)) {
		// Print all device information
		printDeviceInfo(deviceList, printer, DEVICE_FUNCTION_TYPE_ALL);
	} else if (discCmds[discCmdType::DISC_PF].enabled || discCmds[discCmdType::DISC_PHYSICALFUNCTION].enabled) {
		printDeviceInfo(deviceList, printer, DEVICE_FUNCTION_TYPE_PHYSICAL);
	} else if (discCmds[discCmdType::DISC_VF].enabled || discCmds[discCmdType::DISC_VIRTUALFUNCTION].enabled) {
		printDeviceInfo(deviceList, printer, DEVICE_FUNCTION_TYPE_VIRTUAL);
	} else {
		// Iterate through the device list and execute the command
		auto jsonObj = std::make_unique<nlohmann::ordered_json>();
		for (auto &device : deviceList) {

			// Call the appropriate command function based on the command type
			for (const auto &cmd : discCmds) {
				if (cmd.second.enabled && cmd.second.func != nullptr) {
					// If there is a heading function, call it first
					if (cmd.second.headingFunc != nullptr && headingFirst) {
						headingFirst = false;

						result = (this->*cmd.second.headingFunc)(jsonObj.get());
						if (result != ZE_RESULT_SUCCESS) {
							return result;
						}
					}

					DBG("Running command: %s\n", cmd.second.opt.name);
					result = (this->*cmd.second.func)(&device, jsonObj.get());
					if (result != ZE_RESULT_SUCCESS) {
						return result;
					}
				}
			}
		}
		printer->print(jsonObj.get());
	}
	return 0;
}
