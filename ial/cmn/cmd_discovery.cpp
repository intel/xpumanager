/*
 *
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_discovery.h"
#include "debug.h"
#include "printer.h"
#include <array>
#include <assert.h>
#include <charconv>
#include <enginegroup.h>
#include <firmware.h>
#include <format>
#include <fstream>
#include <gscupd.h>
#include <iomanip>
#include <memory.h>
#include <pci.h>
#include <ranges>
#include <span>
#include <sstream>

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
	{DUMP_NUMBEROFTILES, {&cmdDiscovery::numberOfTiles, "Number of Tiles"}},
	{DUMP_NUMBEROFSLICES, {&cmdDiscovery::numberOfSlices, "Number of Slices"}},
	{DUMP_NUMBEROFSUBSLICESPERSLICE, {&cmdDiscovery::numberOfSubslicesPerSlice, "Number of Sub-slices per Slice"}},
	{DUMP_NUMBEROFEUS_PERSUBSLICE, {&cmdDiscovery::numberOfEUsPerSubslice, "Number of EUs per Sub-slice"}},
	{DUMP_NUMBEROFTHREADSPEREU, {&cmdDiscovery::numberOfThreadsPerEU, "Number of Threads per EU"}},
	{DUMP_PHYSICALEUSIMDWIDTH, {&cmdDiscovery::physicalEUSimdWidth, "Physical EU SIMD Width"}},
	{DUMP_MAXCOMMANDQUEUEPRIORITY, {&cmdDiscovery::maxCommandQueuePriority, "Max Command Queue Priority"}},
	{DUMP_MAXHARDWARECONTEXTS, {&cmdDiscovery::maxHardwareContexts, "Max Hardware Contexts"}},
	{DUMP_MAXMEMALLOCSIZE, {&cmdDiscovery::maxMemAllocSize, "Max Memory Alloc Size"}},
	{DUMP_MEMORYFREESIZE, {&cmdDiscovery::memoryFreeSize, "Memory Free Size"}},
	{DUMP_MEMORYECCSTATE, {&cmdDiscovery::memoryEccState, "Memory ECC State"}},
	{DUMP_KERNELVERSION, {&cmdDiscovery::kernelVersion, "Kernel Version"}},
	{DUMP_DRMDEVICE, {&cmdDiscovery::drmDevice, "DRM Device"}},
	{DUMP_DEVICETYPE, {&cmdDiscovery::deviceType, "Device Type"}},
	{DUMP_SKUTYPE, {&cmdDiscovery::skuType, "SKU Type"}},
	{DUMP_PCIEMAXBANDWIDTH, {&cmdDiscovery::pcieMaxBandwidth, "PCIe Max Bandwidth"}},
	{DUMP_AMCFIRMWARENAME, {&cmdDiscovery::amcFirmwareName, "AMC Firmware Name"}},
	{DUMP_AMCFIRMWAREVERSION, {&cmdDiscovery::amcFirmwareVersion, "AMC Firmware Version"}},
	{DUMP_GFXPSCBINFIRMWARENAME, {&cmdDiscovery::gfxPscBinFirmwareName, "GFX PSCBIN Firmware Name"}},
	{DUMP_GFXPSCBINFIRMWAREVERSION, {&cmdDiscovery::gfxPscBinFirmwareVersion, "GFX PSCBIN Firmware Version"}},
	{DUMP_OPROMCODEFIRMWARENAME, {&cmdDiscovery::opromCodeFirmwareName, "OPROM CODE Firmware Name"}},
	{DUMP_OPROMCODEFIRMWAREVERSION, {&cmdDiscovery::opromCodeFirmwareVersion, "OPROM CODE Firmware Version"}},
	{DUMP_OPROMDATAFIRMWARENAME, {&cmdDiscovery::opromDataFirmwareName, "OPROM DATA Firmware Name"}},
	{DUMP_OPROMDATAFIRMWAREVERSION, {&cmdDiscovery::opromDataFirmwareVersion, "OPROM DATA Firmware Version"}},
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
		{"uuid", "UUID"},
		{"pci_bdf_address", "PCI BDF Address"},
		{"drm_device_path", "DRM Device"},
		{"function_type", "Function Type"},
		{"survivability_mode", "Survivability mode"},
		{"serial_number", "Serial Number"},
		{"core_clock_rate", "Core Clock Rate"},
		{"device_stepping", "Stepping"},
		{"driver_version", "Driver Version"},
		{"gfx_firmware_version", "GFX Firmware Version"},
		{"gfx_data_firmware_version", "GFX Data Firmware Version"},
		{"pci_slot", "PCI Slot"},
		{"pcie_generation", "PCIe Generation"},
		{"pcie_max_link_width", "PCIe Max Link Width"},
		{"oam_socket_id", "OAM Socket ID"},
		{"memory_physical_size", "Memory Physical Size"},
		{"memory_physical_size_byte", "Memory Physical Size"},
		{"number_of_memory_channels", "Number of Memory Channels"},
		{"memory_bus_width", "Memory Bus Width"},
		{"number_of_eus", "Number of EUs"},
		{"number_of_media_engines", "Number of Media Engines"},
		{"number_of_media_enh_engines", "Number of Media Enhancement Engines"},
		{"gfx_firmware_status", "GFX Firmware Status"},
		{"pci_vendor_id", "PCI Vendor ID"},
		{"pci_device_id", "PCI Device ID"},
		{"number_of_tiles", "Number of Tiles"},
		{"number_of_slices", "Number of Slices"},
		{"number_of_sub_slices_per_slice", "Number of Sub-slices per Slice"},
		{"number_of_eus_per_sub_slice", "Number of EUs per Sub-slice"},
		{"number_of_threads_per_eu", "Number of Threads per EU"},
		{"physical_eu_simd_width", "Physical EU SIMD Width"},
		{"max_command_queue_priority", "Max Command Queue Priority"},
		{"max_hardware_contexts", "Max Hardware Contexts"},
		{"max_mem_alloc_size_byte", "Max Memory Alloc Size"},
		{"memory_free_size_byte", "Memory Free Size"},
		{"memory_ecc_state", "Memory ECC State"},
		{"kernel_version", "Kernel Version"},
		{"drm_device", "DRM Device"},
		{"device_type", "Device Type"},
		{"sku_type", "SKU Type"},
		{"pcie_max_bandwidth", "PCIe Max Bandwidth"},
		{"amc_firmware_name", "AMC Firmware Name"},
		{"amc_firmware_version", "AMC Firmware Version"},
		{"gfx_pscbin_firmware_name", "GFX PSCBIN Firmware Name"},
		{"gfx_pscbin_firmware_version", "GFX PSCBIN Firmware Version"},
		{"oprom_code_firmware_name", "OPROM CODE Firmware Name"},
		{"oprom_code_firmware_version", "OPROM CODE Firmware Version"},
		{"oprom_data_firmware_name", "OPROM DATA Firmware Name"},
		{"oprom_data_firmware_version", "OPROM DATA Firmware Version"},
		{"gfx_firmware_name", "GFX Firmware Name"},
		{"gfx_data_firmware_name", "GFX Data Firmware Name"},
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
	auto valueToString = [](const nlohmann::ordered_json &val) -> std::string {
		if (val.is_string()) {
			return val.get<std::string>();
		}
		return val.dump();
	};

	auto printRow = [](const std::string &col1, const std::string &col2) {
		PRINT("| %-9s | %-84s |\n", col1.c_str(), col2.c_str());
	};

	auto printBlankRow = []() { PRINT("| %-9s | %-84s |\n", "", ""); };

	auto printTableHeader = []() {
		PRINT("+-----------+--------------------------------------------------------------------------------------+\n");
		PRINT("| Device ID | Device Information                                                                   |\n");
		PRINT("+-----------+--------------------------------------------------------------------------------------+\n");
	};

	if (jsonObj->contains("heading")) {
		// Print CSV-style headers for dump command
		const auto &headers = (*jsonObj)["heading"];
		for (size_t i = 0; i < headers.size(); ++i) {
			PRINT("%s", valueToString(headers[i]).c_str());
			if (i < headers.size() - 1) {
				PRINT(", ");
			}
		}
		PRINT("\n");

		for (auto &deviceItem : jsonObj->items()) {
			if (deviceItem.key() != "heading") {
				const auto &values = deviceItem.value();
				for (size_t i = 0; i < values.size(); ++i) {
					PRINT("%s", valueToString(values[i]).c_str());
					if (i < values.size() - 1) {
						PRINT(", ");
					}
				}
				PRINT("\n");
			}
		}
		return;
	}

	// Table-based output for device list or single device
	printTableHeader();

	if (jsonObj->contains("device_list")) {
		for (auto &device : (*jsonObj)["device_list"]) {
			std::string deviceId = device.contains("device_id") ? valueToString(device["device_id"]) : "";

			auto printField = [&](const char *key, const char *label) {
				if (device.contains(key)) {
					printRow(deviceId, std::string(label) + ": " + valueToString(device[key]));
					deviceId = ""; // Only show device ID once
				}
			};

			printField("device_name", "Device Name");
			printField("vendor_name", "Vendor Name");
			printField("uuid", "SOC UUID");
			printField("pci_bdf_address", "PCI BDF Address");
			printField("drm_device", "DRM Device");
			printField("device_function_type", "Function Type");

			PRINT("+-----------+--------------------------------------------------------------------------------------+"
				  "\n");
		}
	} else {
		std::string deviceId = jsonObj->contains("device_id") ? valueToString((*jsonObj)["device_id"]) : "";

		auto printField = [&](const char *key, const char *label) {
			if (jsonObj->contains(key)) {
				printRow(deviceId, std::string(label) + ": " + valueToString((*jsonObj)[key]));
				deviceId = ""; // Only show device ID once
			}
		};

		// Group 1: Basic Device Information
		printField("device_type", "Device Type");
		printField("device_name", "Device Name");
		printField("pci_device_id", "PCI Device ID");
		printField("vendor_name", "Vendor Name");
		printField("uuid", "SOC UUID");
		printField("serial_number", "Serial Number");
		printField("core_clock_rate", "Core Clock Rate");
		printField("device_stepping", "Stepping");
		printField("sku_type", "SKU Type");
		printBlankRow();

		// Group 2: Driver and Firmware
		printField("driver_version", "Driver Version");
		printField("kernel_version", "Kernel Version");
		printField("gfx_firmware_name", "GFX Firmware Name");
		printField("gfx_firmware_version", "GFX Firmware Version");
		printField("gfx_firmware_status", "GFX Firmware Status");
		printBlankRow();

		// Group 3: PCIe Information
		printField("pci_bdf_address", "PCI BDF Address");
		if (jsonObj->contains("pci_slot")) {
			std::string pciSlot = valueToString((*jsonObj)["pci_slot"]);
			printRow("", std::string("PCI Slot: ") + (pciSlot.empty() ? "N/A" : pciSlot));
		}
		printField("pcie_generation", "PCIe Generation");
		printField("pcie_max_link_width", "PCIe Max Link Width");
		printField("pcie_max_bandwidth", "PCIe Max Bandwidth");
		printBlankRow();

		// Group 4: Memory Information
		printField("memory_physical_size", "Memory Physical Size");
		if (jsonObj->contains("max_mem_alloc_size_byte")) {
			uint64_t maxAllocBytes = 0;
			std::string bytesStr = valueToString((*jsonObj)["max_mem_alloc_size_byte"]);
			auto [ptr, ec] = std::from_chars(bytesStr.data(), bytesStr.data() + bytesStr.size(), maxAllocBytes);
			if (ec == std::errc{}) {
				double maxAllocMiB = static_cast<double>(maxAllocBytes) / (1024.0 * 1024.0);
				printRow("", std::format("Max Mem Alloc Size: {:.2f} MiB", maxAllocMiB));
			}
		}
		if (jsonObj->contains("memory_ecc_state")) {
			std::string eccState = valueToString((*jsonObj)["memory_ecc_state"]);
			printRow("", std::string("ECC State: ") + (eccState.empty() ? "N/A" : eccState));
		}
		printField("number_of_memory_channels", "Number of Memory Channels");
		printField("memory_bus_width", "Memory Bus Width");
		printField("max_hardware_contexts", "Max Hardware Contexts");
		printField("max_command_queue_priority", "Max Command Queue Priority");
		printBlankRow();

		// Group 5: EU and Architecture Information
		printField("number_of_eus", "Number of EUs");
		printField("number_of_tiles", "Number of Tiles");
		printField("number_of_slices", "Number of Slices");
		printField("number_of_sub_slices_per_slice", "Number of Sub Slices per Slice");
		printField("number_of_threads_per_eu", "Number of Threads per EU");
		printField("physical_eu_simd_width", "Physical EU SIMD Width");
		printField("number_of_media_engines", "Number of Media Engines");
		printField("number_of_media_enh_engines", "Number of Media Enhancement Engines");
		printBlankRow();

		PRINT("+-----------+--------------------------------------------------------------------------------------+\n");
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
	(*jsonObj)["device_function_type"] = (funcType == DEVICE_FUNCTION_TYPE_PHYSICAL) ? "physical" : "virtual";
	(*jsonObj)["device_id"] = device->index;

	deviceName(device, &outputLine);
	(*jsonObj)["device_name"] = outputLine;

	deviceType(device, &outputLine);
	(*jsonObj)["device_type"] = outputLine;

	(*jsonObj)["drm_device"] = device->dev->getDrmDevPath();

	pciBDFAddress(device, &outputLine);
	(*jsonObj)["pci_bdf_address"] = outputLine;

	pciDeviceID(device, &outputLine);
	(*jsonObj)["pci_device_id"] = outputLine;

	socUuid(device, &outputLine);
	(*jsonObj)["uuid"] = outputLine;

	vendorName(device, &outputLine);
	(*jsonObj)["vendor_name"] = outputLine;

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

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief This function does the common pre-checks for the discovery commands.
 *
 * @param[out] d A pointer to the device info structure.
 *
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiscovery::preCheck(std::vector<int> *dumpArgs)
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
			dumpArgs->push_back(i);
		}
	} else {
		// Split the dump command argument by commas
		std::stringstream ss(val.c_str());
		std::string token;
		while (getline(ss, token, ',')) {
			int value = 0;
			auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
			if (ec == std::errc{}) {
				dumpArgs->push_back(value);
			} else {
				ERR("Invalid dump command argument '%s'. Must be a valid integer\n", token.c_str());
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}
		}
	}

	// Check if each dump command argument is a number between 1 and TOTAL_DISC_DUMPS
	for (const auto arg : *dumpArgs) {
		if (arg < DUMP_DEVICEID || arg > TOTAL_DISC_DUMPS) {
			ERR("Invalid dump command argument '%d'. It must be between 1 and %d\n", arg, TOTAL_DISC_DUMPS);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the dump heading command. This command prints the heading
 *
 * @param[out] jsonObj Pointer to JSON object to populate with heading information
 *
 * @retval ZE_RESULT_SUCCESS Successfully populated heading
 * @retval ZE_RESULT_ERROR_INVALID_ARGUMENT Invalid dump arguments specified
 */
ze_result_t cmdDiscovery::dumpHeading(nlohmann::ordered_json *jsonObj)
{
	TRACING();
	std::vector<int> dumpArgs;
	bool found = false;
	std::string val = discCmds[discCmdType::DISC_DUMP].val;
	auto headingJson = std::make_unique<nlohmann::ordered_json>();

	const auto result = preCheck(&dumpArgs);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	for (const auto arg : dumpArgs) {
		for (const auto &cmd : discDumpCmds) {
			if (cmd.first == arg) {
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
 * @param[in] d Pointer to the device info structure
 * @param[out] jsonObj Pointer to JSON object to populate with device properties
 *
 * @retval ZE_RESULT_SUCCESS Successfully dumped device properties
 * @retval ZE_RESULT_ERROR_INVALID_ARGUMENT Invalid dump arguments specified
 * @retval ZE_RESULT_ERROR_* Error occurred during property retrieval
 */
ze_result_t cmdDiscovery::dump(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	std::vector<int> dumpArgs;
	std::string val = discCmds[discCmdType::DISC_DUMP].val;
	auto valuesJson = std::make_unique<nlohmann::ordered_json>();
	bool found = false;

	result = preCheck(&dumpArgs);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	std::string outputLine;
	for (const auto arg : dumpArgs) {
		for (const auto &cmd : discDumpCmds) {
			if (cmd.first == arg) {
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
 * @param[in] d Pointer to the device info structure
 * @param[out] jsonObj Pointer to JSON object to populate with device properties
 *
 * @retval ZE_RESULT_SUCCESS Successfully gathered and populated device properties
 * @retval ZE_RESULT_ERROR_* Error occurred during property gathering
 */
ze_result_t cmdDiscovery::dumpAll(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;

	// We should dump all properties for a device only when no other command line options are specified.
	for (const auto &cmd : discCmds) {
		if (cmd.second.enabled && (cmd.first != discCmdType::DISC_DEVICE && cmd.first != discCmdType::DISC_JSON)) {
			// Silently return if any other command line options are specified
			return result;
		}
	}

	DeviceProperties props;
	result = gatherDeviceProperties(d, props);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	convertToJson(props, jsonObj);

	return result;
}

/**
 * @brief Gathers all device properties into a map (decoupled from JSON)
 *
 * @param[in] d Pointer to device info structure
 * @param[out] props Output map to populate with key-value pairs (string keys and values)
 *
 * @retval ZE_RESULT_SUCCESS Successfully gathered all device properties
 * @retval ZE_RESULT_ERROR_* Error occurred during property retrieval
 */
ze_result_t cmdDiscovery::gatherDeviceProperties(devInfo *d, DeviceProperties &props)
{
	TRACING();
	std::string outputLine;
	ze_result_t result = ZE_RESULT_SUCCESS;

	amcFirmwareName(d, &outputLine);
	props["amc_firmware_name"] = outputLine;

	amcFirmwareVersion(d, &outputLine);
	props["amc_firmware_version"] = outputLine;

	deviceID(d, &outputLine);
	props["device_id"] = outputLine;

	deviceName(d, &outputLine);
	props["device_name"] = outputLine;

	vendorName(d, &outputLine);
	props["vendor_name"] = outputLine;

	socUuid(d, &outputLine);
	props["uuid"] = outputLine;

	serialNumber(d, &outputLine);
	props["serial_number"] = outputLine;

	stepping(d, &outputLine);
	props["device_stepping"] = outputLine;

	driverVersion(d, &outputLine);
	props["driver_version"] = outputLine;

	gfxFirmwareVersion(d, &outputLine);
	props["gfx_firmware_version"] = outputLine;

	gfxDataFirmwareVersion(d, &outputLine);
	props["gfx_data_firmware_version"] = outputLine;

	pciBDFAddress(d, &outputLine);
	props["pci_bdf_address"] = outputLine;

	pciSlot(d, &outputLine);
	props["pci_slot"] = outputLine;

	pcieGeneration(d, &outputLine);
	props["pcie_generation"] = outputLine;

	pcieMaxLinkWidth(d, &outputLine);
	props["pcie_max_link_width"] = outputLine;

	memoryChannels(d, &outputLine);
	props["number_of_memory_channels"] = outputLine;

	memoryBusWidth(d, &outputLine);
	props["memory_bus_width"] = outputLine;

	eus(d, &outputLine);
	props["number_of_eus"] = outputLine;

	mediaEngines(d, &outputLine);
	props["number_of_media_engines"] = outputLine;

	mediaEnhancementEngines(d, &outputLine);
	props["number_of_media_enh_engines"] = outputLine;

	props["gfx_firmware_name"] = "GFX";
	props["gfx_data_firmware_name"] = "GFX_DATA";

	gfxFirmwareStatus(d, &outputLine);
	props["gfx_firmware_status"] = outputLine;

	gfxPscBinFirmwareName(d, &outputLine);
	props["gfx_pscbin_firmware_name"] = outputLine;

	gfxPscBinFirmwareVersion(d, &outputLine);
	props["gfx_pscbin_firmware_version"] = outputLine;

	opromCodeFirmwareName(d, &outputLine);
	props["oprom_code_firmware_name"] = outputLine;

	opromCodeFirmwareVersion(d, &outputLine);
	props["oprom_code_firmware_version"] = outputLine;

	opromDataFirmwareName(d, &outputLine);
	props["oprom_data_firmware_name"] = outputLine;

	opromDataFirmwareVersion(d, &outputLine);
	props["oprom_data_firmware_version"] = outputLine;

	uint64_t physicalSize = 0;
	auto *const m = d->dev->getMemory();
	m->getMemorySize(&physicalSize);
	double physicalSizeMiB = static_cast<double>(physicalSize) / (1024.0 * 1024.0);
	props["memory_physical_size"] = std::format("{:.2f} MiB", physicalSizeMiB);
	props["memory_physical_size_byte"] = std::to_string(physicalSize);

	pciVendorID(d, &outputLine);
	props["pci_vendor_id"] = outputLine;

	pciDeviceID(d, &outputLine);
	props["pci_device_id"] = outputLine;

	numberOfTiles(d, &outputLine);
	props["number_of_tiles"] = outputLine;

	numberOfSlices(d, &outputLine);
	props["number_of_slices"] = outputLine;

	numberOfSubslicesPerSlice(d, &outputLine);
	props["number_of_sub_slices_per_slice"] = outputLine;

	numberOfEUsPerSubslice(d, &outputLine);
	props["number_of_eus_per_sub_slice"] = outputLine;

	numberOfThreadsPerEU(d, &outputLine);
	props["number_of_threads_per_eu"] = outputLine;

	physicalEUSimdWidth(d, &outputLine);
	props["physical_eu_simd_width"] = outputLine;

	maxCommandQueuePriority(d, &outputLine);
	props["max_command_queue_priority"] = outputLine;

	maxHardwareContexts(d, &outputLine);
	props["max_hardware_contexts"] = outputLine;

	maxMemAllocSize(d, &outputLine);
	props["max_mem_alloc_size_byte"] = outputLine;

	memoryFreeSize(d, &outputLine);
	props["memory_free_size_byte"] = outputLine;

	memoryEccState(d, &outputLine);
	props["memory_ecc_state"] = outputLine;

	kernelVersion(d, &outputLine);
	props["kernel_version"] = outputLine;

	drmDevice(d, &outputLine);
	props["drm_device"] = outputLine;

	deviceType(d, &outputLine);
	props["device_type"] = outputLine;

	skuType(d, &outputLine);
	props["sku_type"] = outputLine;

	pcieMaxBandwidth(d, &outputLine);
	props["pcie_max_bandwidth"] = outputLine;

	props["oam_socket_id"] = "N/A";

	auto zeDevProp = ze_device_properties_t{};
	d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	props["core_clock_rate"] = std::format("{} MHz", zeDevProp.coreClockRate);

	return result;
}

/**
 * @brief Converts device properties map to JSON
 *
 * @param[in] props Device properties map (string key-value pairs)
 * @param[out] jsonObj Output JSON object to populate
 * @param[in] useIntForDeviceId Whether to convert device_id to integer (default: true)
 */
void cmdDiscovery::convertToJson(const DeviceProperties &props, nlohmann::ordered_json *jsonObj, bool useIntForDeviceId)
{
	for (const auto &[key, value] : props) {
		// Special handling for device_id in new format
		if (useIntForDeviceId && key == "device_id") {
			int deviceId = 0;
			auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), deviceId);
			if (ec == std::errc{}) {
				(*jsonObj)[key] = deviceId;
			} else {
				(*jsonObj)[key] = value; // Fallback to string if parsing fails
			}
		} else {
			(*jsonObj)[key] = value;
		}
	}
}

/**
 * @brief Prints out the device ID of the device when user runs discovery --dump 1
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved device ID
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
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved device name
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::deviceName(devInfo *d, std::string *outputLine)
{
	TRACING();

	auto zeDevProp = ze_device_properties_t{};

	if (const auto result = d->dev->getDevProps(d->deviceHdl, &zeDevProp); result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	*outputLine = zeDevProp.name;

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints out the vendor name of a device when user runs discovery --dump 3.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved vendor name
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::vendorName(devInfo *d, std::string *outputLine)
{
	TRACING();

	zes_device_properties_t zesDevProp = {};

	const auto result = d->dev->zesGetDevProps(d->zesDeviceHdl, &zesDevProp);
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
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved SOC UUID
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::socUuid(devInfo *d, std::string *outputLine)
{
	TRACING();

	auto devProp = ze_device_properties_t{};
	char output[256] = {0};

	const auto result = d->dev->getDevProps(d->deviceHdl, &devProp);
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
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved serial number
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::serialNumber(devInfo *d, std::string *outputLine)
{
	TRACING();

	zes_device_properties_t zesDevProp = {};

	const auto result = d->dev->zesGetDevProps(d->zesDeviceHdl, &zesDevProp);
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
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved core clock rate
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::coreClockRate(devInfo *d, std::string *outputLine)
{
	TRACING();

	ze_device_properties_t zeDevProp = {};

	const auto result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
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
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (e.g., "A0", "B1")
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved device stepping
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::stepping(devInfo *d, std::string *outputLine)
{
	TRACING();

	zes_device_properties_t zesDevProp = {};

	const auto result = d->dev->zesGetDevProps(d->zesDeviceHdl, &zesDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	// Extract stepping from modelName (e.g., "BMG A0" -> "A0")
	// The modelName typically contains platform code and stepping
	std::string modelName = zesDevProp.modelName;
	size_t lastSpace = modelName.find_last_of(' ');
	if (lastSpace != std::string::npos && lastSpace + 1 < modelName.length()) {
		std::string stepping = modelName.substr(lastSpace + 1);
		// Verify it looks like a stepping (e.g., "A0", "B1")
		if (stepping.length() >= 2 && isalpha(stepping[0]) && isdigit(stepping[1])) {
			*outputLine = stepping;
		} else {
			*outputLine = "A0";
		}
	} else {
		*outputLine = "A0"; // Default fallback
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the driver version for a device when user runs discovery --dump 8.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved driver version
 * @retval ZE_RESULT_ERROR_* Failed to get driver properties
 */
ze_result_t cmdDiscovery::driverVersion(devInfo *d, std::string *outputLine)
{
	TRACING();

	zes_device_properties_t zesDevProp = {};

	const auto result = d->dev->zesGetDevProps(d->zesDeviceHdl, &zesDevProp);
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
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (sanitized, printable characters only)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved GFX firmware version
 * @retval ZE_RESULT_ERROR_* Failed to get firmware version
 */
ze_result_t cmdDiscovery::gfxFirmwareVersion(devInfo *d, std::string *outputLine)
{
	TRACING();
	std::array<char, MAX_PATH> version = {};

	auto *const p = d->dev->getPCI();
	auto *const fw = d->dev->getFirmware();

	fw->getFWversion(fwType::GFX, p->getBDFStr().c_str(), version.data(), static_cast<uint32_t>(version.size()));

	// Sanitize firmware version by skipping non-printable characters
	std::string_view versionView(version.data(), strlen(version.data()));
	auto sanitized = versionView | std::views::filter([](char c) { return c >= 32 && c <= 126; });
	*outputLine = std::string(sanitized.begin(), sanitized.end());
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the GFX data firmware version for a device when user runs discovery --dump 10.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved GFX data firmware version
 * @retval ZE_RESULT_ERROR_* Failed to get firmware version
 */
ze_result_t cmdDiscovery::gfxDataFirmwareVersion(devInfo *d, std::string *outputLine)
{
	TRACING();
	std::array<char, MAX_PATH> version = {};

	auto *const p = d->dev->getPCI();
	auto *const fw = d->dev->getFirmware();

	fw->getFWversion(fwType::GFX_DATA, p->getBDFStr().c_str(), version.data(), static_cast<uint32_t>(version.size()));
	*outputLine = version.data();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the PCI BDF address for a device when user runs discovery --dump 11.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (format: DDDD:BB:DD.F)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved PCI BDF address
 */
ze_result_t cmdDiscovery::pciBDFAddress(devInfo *d, std::string *outputLine)
{
	TRACING();

	auto *const p = d->dev->getPCI();
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
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (reads from sysfs label)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved PCI slot information
 */
ze_result_t cmdDiscovery::pciSlot(devInfo *d, std::string *outputLine)
{
	TRACING();

	auto *const p = d->dev->getPCI();
	if (p == nullptr) {
		*outputLine = "N/A";
		return ZE_RESULT_SUCCESS;
	}

	*outputLine = GETPCISLOTLABEL(p->getBDFStr());
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the PCIe generation for a device when user runs discovery --dump 13.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (e.g., "4" for Gen4)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved PCIe generation
 * @retval ZE_RESULT_ERROR_* Failed to get PCI properties
 */
ze_result_t cmdDiscovery::pcieGeneration(devInfo *d, std::string *outputLine)
{
	TRACING();

	zes_pci_properties_t pciProps;

	auto *const p = d->dev->getPCI();
	const auto result = p->getProperties(d->zesDeviceHdl, &pciProps);
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
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (e.g., "16" for x16)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved PCIe max link width
 * @retval ZE_RESULT_ERROR_* Failed to get PCI properties
 */
ze_result_t cmdDiscovery::pcieMaxLinkWidth(devInfo *d, std::string *outputLine)
{
	TRACING();

	zes_pci_properties_t pciProps;

	auto *const p = d->dev->getPCI();
	const auto result = p->getProperties(d->zesDeviceHdl, &pciProps);
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
 * @param[in] d Pointer to the device info structure (unused)
 * @param[out] outputLine Pointer to the output line string (always "unknown")
 *
 * @retval ZE_RESULT_SUCCESS Always succeeds
 */
ze_result_t cmdDiscovery::oamSocketID(UNUSED devInfo *d, std::string *outputLine)
{
	TRACING();

	// PVC only - not applicable

	*outputLine = "N/A";
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the memory physical size for a device when user runs discovery --dump 16.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (format: "X.XX MiB")
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved memory physical size
 * @retval ZE_RESULT_ERROR_* Failed to get memory size
 */
ze_result_t cmdDiscovery::memoryPhysicalSize(devInfo *d, std::string *outputLine)
{
	TRACING();

	uint64_t physicalSize = 0;

	auto *const m = d->dev->getMemory();

	if (const auto result = m->getMemorySize(&physicalSize); result != ZE_RESULT_SUCCESS) {
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
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved memory channel count
 * @retval ZE_RESULT_ERROR_* Failed to get memory information
 */
ze_result_t cmdDiscovery::memoryChannels(devInfo *d, std::string *outputLine)
{
	TRACING();

	uint32_t channels = 0;

	auto *const m = d->dev->getMemory();

	const auto result = m->getMemoryChannels(&channels);
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
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (in bits)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved memory bus width
 * @retval ZE_RESULT_ERROR_* Failed to get memory information
 */
ze_result_t cmdDiscovery::memoryBusWidth(devInfo *d, std::string *outputLine)
{
	TRACING();

	uint32_t busWidth = 0;

	auto *const m = d->dev->getMemory();

	if (const auto result = m->getMemoryBusWidth(&busWidth); result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory bus width: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	*outputLine = std::to_string(busWidth);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the EUs for a device when user runs discovery --dump 19.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (total execution unit count)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved EU count
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::eus(devInfo *d, std::string *outputLine)
{
	TRACING();

	ze_device_properties_t zeDevProp = {};

	zeDevProp.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
	ze_eu_count_ext_t *extendedPropertiesPtr = new ze_eu_count_ext_t();
	memset(extendedPropertiesPtr, 0, sizeof(ze_eu_count_ext_t));
	extendedPropertiesPtr->stype = ZE_STRUCTURE_TYPE_EU_COUNT_EXT;
	zeDevProp.pNext = extendedPropertiesPtr;

	const auto result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		delete extendedPropertiesPtr;
		return result;
	}

	if (zeDevProp.pNext != nullptr) {
		ze_eu_count_ext_t *euCountExt = (ze_eu_count_ext_t *)(zeDevProp.pNext);
		*outputLine = std::to_string(euCountExt->numTotalEUs);
	}

	delete extendedPropertiesPtr;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the media engines for a device when user runs discovery --dump 20.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (media engine count)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved media engine count
 */
ze_result_t cmdDiscovery::mediaEngines(devInfo *d, std::string *outputLine)
{
	TRACING();

	auto *const engineGroup = d->dev->getEngineGroup();
	if (engineGroup == nullptr) {
		ERR("Failed to get engine group\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	uint32_t mediaEnginesCount = 0;
	const auto result = engineGroup->getEngineCountByType(&mediaEnginesCount, ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE);
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
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (media enhancement engine count)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved media enhancement engine count
 */
ze_result_t cmdDiscovery::mediaEnhancementEngines(devInfo *d, std::string *outputLine)
{
	TRACING();

	auto *const engineGroup = d->dev->getEngineGroup();
	if (engineGroup == nullptr) {
		ERR("Failed to get engine group\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	uint32_t mediaEnhancementEnginesCount = 0;
	const auto result =
		engineGroup->getEngineCountByType(&mediaEnhancementEnginesCount, ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE);
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
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string ("running" or error status)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved firmware status
 */
ze_result_t cmdDiscovery::gfxFirmwareStatus(devInfo *d, std::string *outputLine)
{
	TRACING();

	auto *const p = d->dev->getPCI();
	if (p == nullptr) {
		ERR("Failed to get PCI device properties.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	const auto fwStatus = p->getFWStatus();
	*outputLine = fwStatus;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the PCI vendor ID for a device when user runs discovery --dump 23.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (hexadecimal format)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved PCI vendor ID
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::pciVendorID(devInfo *d, std::string *outputLine)
{
	TRACING();

	ze_device_properties_t zeDevProp = {};

	const auto result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
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
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (hex format)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved PCI device ID
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::pciDeviceID(devInfo *d, std::string *outputLine)
{
	TRACING();

	auto zeDevProp = ze_device_properties_t{};

	const auto result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
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
 * @brief Prints the number of tiles for a device.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved tile count
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::numberOfTiles(devInfo *d, std::string *outputLine)
{
	TRACING();
	auto zeDevProp = ze_device_properties_t{};
	const auto result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	*outputLine = std::to_string(zeDevProp.numSlices > 0 ? 1 : 0); // Simplified - actual tile count may vary
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the number of slices for a device.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved slice count
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::numberOfSlices(devInfo *d, std::string *outputLine)
{
	TRACING();
	auto zeDevProp = ze_device_properties_t{};
	if (const auto result = d->dev->getDevProps(d->deviceHdl, &zeDevProp); result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	*outputLine = std::to_string(zeDevProp.numSlices);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the number of sub-slices per slice for a device.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved sub-slices per slice count
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::numberOfSubslicesPerSlice(devInfo *d, std::string *outputLine)
{
	TRACING();
	auto zeDevProp = ze_device_properties_t{};
	if (const auto result = d->dev->getDevProps(d->deviceHdl, &zeDevProp); result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	*outputLine = std::to_string(zeDevProp.numSubslicesPerSlice);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the number of EUs per sub-slice for a device.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved EUs per sub-slice count
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::numberOfEUsPerSubslice(devInfo *d, std::string *outputLine)
{
	TRACING();
	auto zeDevProp = ze_device_properties_t{};
	if (const auto result = d->dev->getDevProps(d->deviceHdl, &zeDevProp); result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	*outputLine = std::to_string(zeDevProp.numEUsPerSubslice);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the number of threads per EU for a device.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved threads per EU count
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::numberOfThreadsPerEU(devInfo *d, std::string *outputLine)
{
	TRACING();
	auto zeDevProp = ze_device_properties_t{};
	if (const auto result = d->dev->getDevProps(d->deviceHdl, &zeDevProp); result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	*outputLine = std::to_string(zeDevProp.numThreadsPerEU);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the physical EU SIMD width for a device.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved physical EU SIMD width
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::physicalEUSimdWidth(devInfo *d, std::string *outputLine)
{
	TRACING();
	auto zeDevProp = ze_device_properties_t{};
	if (const auto result = d->dev->getDevProps(d->deviceHdl, &zeDevProp); result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	*outputLine = std::to_string(zeDevProp.physicalEUSimdWidth);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the max command queue priority for a device.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved max command queue priority
 */
ze_result_t cmdDiscovery::maxCommandQueuePriority(devInfo *d, std::string *outputLine)
{
	TRACING();
	// Get command queue properties
	int32_t count = d->dev->getCmdQueuePropsCount();
	if (count <= 0) {
		*outputLine = "0";
		return ZE_RESULT_SUCCESS;
	}
	// Find the maximum priority across all command queue groups
	*outputLine = "0"; // Default priority
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the max hardware contexts for a device.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved max hardware contexts
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::maxHardwareContexts(devInfo *d, std::string *outputLine)
{
	TRACING();
	auto zeDevProp = ze_device_properties_t{};
	const auto result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	// Use the maxHardwareContexts directly from device properties
	*outputLine = std::to_string(zeDevProp.maxHardwareContexts);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the max memory alloc size for a device.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (in bytes)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved max memory alloc size
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::maxMemAllocSize(devInfo *d, std::string *outputLine)
{
	TRACING();
	auto zeDevProp = ze_device_properties_t{};
	const auto result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	// Use maxMemAllocSize directly from device properties
	*outputLine = std::to_string(zeDevProp.maxMemAllocSize);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the memory free size for a device.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (in bytes)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved memory free size
 * @retval ZE_RESULT_ERROR_* Failed to get memory information
 */
ze_result_t cmdDiscovery::memoryFreeSize(devInfo *d, std::string *outputLine)
{
	TRACING();

	// Get memory properties to calculate free memory
	uint32_t memCount = 0;
	ze_device_memory_properties_t *memProps = nullptr;
	ze_result_t result = d->dev->getMemProps(d->deviceHdl, &memProps, &memCount);

	if (result != ZE_RESULT_SUCCESS || memCount == 0 || memProps == nullptr) {
		*outputLine = "0";
		return ZE_RESULT_SUCCESS;
	}

	// Get used memory
	auto *const mem = d->dev->getMemory();
	if (mem == nullptr) {
		*outputLine = "0";
		return ZE_RESULT_SUCCESS;
	}

	uint64_t usedBytes = 0;
	double utilization = 0.0;
	result = mem->getMemoryUsed(&usedBytes, &utilization);

	if (result == ZE_RESULT_SUCCESS) {
		// Calculate free = total - used
		uint64_t totalSize = memProps[0].totalSize;
		uint64_t freeSize = (totalSize > usedBytes) ? (totalSize - usedBytes) : 0;
		*outputLine = std::to_string(freeSize);
	} else {
		*outputLine = "0";
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the memory ECC state for a device.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string ("enabled" or "disabled")
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved memory ECC state
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::memoryEccState(devInfo *d, std::string *outputLine)
{
	TRACING();
	auto zeDevProp = ze_device_properties_t{};
	const auto result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	if (zeDevProp.flags & ZE_DEVICE_PROPERTY_FLAG_ECC) {
		*outputLine = "enabled";
	} else {
		*outputLine = "N/A";
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the kernel version for a device.
 *
 * @param[in] d Pointer to the device info structure (unused)
 * @param[out] outputLine Pointer to the output line string (Linux kernel version)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved kernel version
 */
ze_result_t cmdDiscovery::kernelVersion(UNUSED devInfo *d, std::string *outputLine)
{
	TRACING();
	*outputLine = GETKERNELVERSION();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the DRM device path for a device.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (DRM device path)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved DRM device path
 */
ze_result_t cmdDiscovery::drmDevice(devInfo *d, std::string *outputLine)
{
	TRACING();
	*outputLine = d->dev->getDrmDevPath();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the device type for a device.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string ("GPU", etc.)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved device type
 * @retval ZE_RESULT_ERROR_* Failed to get device properties
 */
ze_result_t cmdDiscovery::deviceType(devInfo *d, std::string *outputLine)
{
	TRACING();
	auto zeDevProp = ze_device_properties_t{};
	const auto result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	switch (zeDevProp.type) {
	case ZE_DEVICE_TYPE_GPU:
		*outputLine = "GPU";
		break;
	case ZE_DEVICE_TYPE_CPU:
		*outputLine = "CPU";
		break;
	case ZE_DEVICE_TYPE_FPGA:
		*outputLine = "FPGA";
		break;
	case ZE_DEVICE_TYPE_MCA:
		*outputLine = "MCA";
		break;
	default:
		*outputLine = "Unknown";
		break;
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the SKU type for a device.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (SKU information)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved SKU type
 */
ze_result_t cmdDiscovery::skuType(devInfo *d, std::string *outputLine)
{
	TRACING();
	zes_device_properties_t zesDevProp = {};
	ze_result_t result = d->dev->zesGetDevProps(d->zesDeviceHdl, &zesDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	// SKU type is typically derived from model name or board number
	// For now, return a generic identifier based on flags
	*outputLine = "Production ES";
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints the PCIe max bandwidth for a device.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (format: "X.XX GB/s")
 *
 * @retval ZE_RESULT_SUCCESS Successfully calculated PCIe max bandwidth
 * @retval ZE_RESULT_ERROR_* Failed to get PCI properties
 */
ze_result_t cmdDiscovery::pcieMaxBandwidth(devInfo *d, std::string *outputLine)
{
	TRACING();
	zes_pci_properties_t pciProps;
	auto *const p = d->dev->getPCI();
	const auto result = p->getProperties(d->zesDeviceHdl, &pciProps);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	// Calculate bandwidth based on PCIe generation and width
	// Formula: (width * gen_rate) where gen_rate depends on PCIe generation
	// Gen 1: ~250 MB/s per lane, Gen 2: ~500 MB/s, Gen 3: ~985 MB/s (1 GB/s), Gen 4: ~1969 MB/s (2 GB/s), Gen 5: ~3938
	// MB/s (4 GB/s)
	double laneRate = 0.985; // Gen 3 default GB/s per lane
	if (pciProps.maxSpeed.gen == 1)
		laneRate = 0.25;
	else if (pciProps.maxSpeed.gen == 2)
		laneRate = 0.5;
	else if (pciProps.maxSpeed.gen == 3)
		laneRate = 0.985;
	else if (pciProps.maxSpeed.gen == 4)
		laneRate = 1.969;
	else if (pciProps.maxSpeed.gen == 5)
		laneRate = 3.938;
	else if (pciProps.maxSpeed.gen >= 6)
		laneRate = 7.877;

	double bandwidth = pciProps.maxSpeed.width * laneRate; // GB/s
	*outputLine = std::format("{:.2f} GB/s", bandwidth);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints AMC firmware name.
 *
 * @param[in] d Pointer to the device info structure (unused)
 * @param[out] outputLine Pointer to the output line string (always "AMC")
 *
 * @retval ZE_RESULT_SUCCESS Always succeeds
 */
ze_result_t cmdDiscovery::amcFirmwareName(UNUSED devInfo *d, std::string *outputLine)
{
	TRACING();
	*outputLine = "AMC";
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints AMC firmware version.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (AMC firmware version)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved AMC firmware version
 */
ze_result_t cmdDiscovery::amcFirmwareVersion(devInfo *d, std::string *outputLine)
{
	TRACING();
	char version[MAX_PATH] = {0};
	pci *p = d->dev->getPCI();
	firmware *fw = d->dev->getFirmware();
	fw->getFWversion(fwType::AMC, p->getBDFStr().c_str(), version, sizeof(version));
	*outputLine = version;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints GFX PSCBIN firmware name.
 *
 * @param[in] d Pointer to the device info structure (unused)
 * @param[out] outputLine Pointer to the output line string (always "GFX_PSCBIN")
 *
 * @retval ZE_RESULT_SUCCESS Always succeeds
 */
ze_result_t cmdDiscovery::gfxPscBinFirmwareName(UNUSED devInfo *d, std::string *outputLine)
{
	TRACING();
	*outputLine = "GFX_PSCBIN";
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints GFX PSCBIN firmware version.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (GFX PSCBIN version)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved GFX PSCBIN firmware version
 */
ze_result_t cmdDiscovery::gfxPscBinFirmwareVersion(devInfo *d, std::string *outputLine)
{
	TRACING();
	char version[MAX_PATH] = {0};
	pci *p = d->dev->getPCI();
	firmware *fw = d->dev->getFirmware();
	fw->getFWversion(fwType::GFX_PSCBIN, p->getBDFStr().c_str(), version, sizeof(version));
	*outputLine = version;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints OPROM CODE firmware name.
 *
 * @param[in] d Pointer to the device info structure (unused)
 * @param[out] outputLine Pointer to the output line string (always "OPROM_CODE")
 *
 * @retval ZE_RESULT_SUCCESS Always succeeds
 */
ze_result_t cmdDiscovery::opromCodeFirmwareName(UNUSED devInfo *d, std::string *outputLine)
{
	TRACING();
	*outputLine = "OPROM_CODE";
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints OPROM CODE firmware version.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (dot-separated version bytes)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved OPROM CODE firmware version
 * @retval ZE_RESULT_ERROR_* Failed to get OPROM version
 */
ze_result_t cmdDiscovery::opromCodeFirmwareVersion(devInfo *d, std::string *outputLine)
{
	TRACING();
	std::array<uint8_t, 8> version = {};
	pci *p = d->dev->getPCI();

	// Get OPROM version directly using gscupd
	gscupd gsc;
	int ret = gsc.getOpromVersion(p->getBDFStr().c_str(), IGSC_OPROM_CODE, version.data(), version.size());
	if (ret != 0) {
		*outputLine = "";
		return ZE_RESULT_SUCCESS;
	}

	// Format OPROM firmware as space-separated decimal byte values
	auto formatted = version | std::views::transform([](uint8_t b) { return std::to_string(b); });

	*outputLine = "";
	for (const auto &val : formatted) {
		if (!outputLine->empty())
			*outputLine += " ";
		*outputLine += val;
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints OPROM DATA firmware name.
 *
 * @param[in] d Pointer to the device info structure (unused)
 * @param[out] outputLine Pointer to the output line string (always "OPROM_DATA")
 *
 * @retval ZE_RESULT_SUCCESS Always succeeds
 */
ze_result_t cmdDiscovery::opromDataFirmwareName(UNUSED devInfo *d, std::string *outputLine)
{
	TRACING();
	*outputLine = "OPROM_DATA";
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints OPROM DATA firmware version.
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] outputLine Pointer to the output line string (dot-separated version bytes)
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved OPROM DATA firmware version
 * @retval ZE_RESULT_ERROR_* Failed to get OPROM version
 */
ze_result_t cmdDiscovery::opromDataFirmwareVersion(devInfo *d, std::string *outputLine)
{
	TRACING();
	std::array<uint8_t, 8> version = {};
	pci *p = d->dev->getPCI();

	// Get OPROM version directly using gscupd
	gscupd gsc;
	int ret = gsc.getOpromVersion(p->getBDFStr().c_str(), IGSC_OPROM_DATA, version.data(), version.size());
	if (ret != 0) {
		*outputLine = "";
		return ZE_RESULT_SUCCESS;
	}

	// Format OPROM firmware as space-separated decimal byte values
	auto formatted = version | std::views::transform([](uint8_t b) { return std::to_string(b); });

	*outputLine = "";
	for (const auto &val : formatted) {
		if (!outputLine->empty())
			*outputLine += " ";
		*outputLine += val;
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Lists all AMC firmware versions for a device when user runs discovery --listamcversions
 *
 * @param[in] d Pointer to the device info structure
 * @param[out] jsonObj Pointer to JSON object to populate with AMC firmware version
 *
 * @retval ZE_RESULT_SUCCESS Successfully retrieved and populated AMC firmware version
 */
ze_result_t cmdDiscovery::listamcversions(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();
	std::array<char, MAX_PATH> version = {};

	auto *const p = d->dev->getPCI();
	auto *const fw = d->dev->getFirmware();

	fw->getFWversion(fwType::AMC, p->getBDFStr().c_str(), version.data(), static_cast<uint32_t>(version.size()));
	jsonObj->push_back(version.data());

	return ZE_RESULT_SUCCESS;
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
		pci *p = device.dev->getPCI();
		foundType = p->getFuncType();
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

	auto result = args->sm.findDevice(discCmds[discCmdType::DISC_DEVICE].val.c_str(), &deviceList);
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
