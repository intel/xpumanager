/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_listpciinfo.h"
#include "debug.h"
#include "table_builder.h"
#include <CLI/CLI.hpp>
#include <format>
#include <memory>

static std::unordered_map<listpciinfoCmdType, listpciinfoCmdStruct> listpciinfoCmds = {
	{listpciinfoCmdType::LISTPCIINFO_HELP, {}},
	{listpciinfoCmdType::LISTPCIINFO_BDF, {}},
	{listpciinfoCmdType::LISTPCIINFO_JSON, {}},
};

/**
 * @brief Prints help for the listpciinfo command
 */
void cmdListpciinfo::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Query PCI properties for xe-bound GPU(s)"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s listpciinfo [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s listpciinfo", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s listpciinfo --bdf [BDF address]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s listpciinfo --bdf [BDF address] -j", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "BDF:      PCI Bus:Device.Function address (e.g. 0000:03:00.0)"));
	helpList.push_back(helpCmd(TITLE, "PCIe Gen: PCIe generation (1-6)"));
	helpList.push_back(helpCmd(TITLE, "Width:    Maximum PCIe link width (lanes)"));
	helpList.push_back(helpCmd(TITLE, "BW(GB/s): Theoretical peak bandwidth in GB/s"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(HEADING, "--bdf                       PCI BDF address to query (e.g. 0000:03:00.0)"));

	printHelp(helpList, helpType);
}

ListgpuTextPrinter::ListgpuTextPrinter() : TextPrinter() {}

/**
 * @brief Formats and prints xedev query results as a human-readable table
 *
 * Expected JSON layout:
 * {
 *   "bdf_device_list": [
 *     { "bdf": "0000:03:00.0", "pci_slot": "PCIEX16(G5)",
 *       "pcie_generation": 5, "max_link_width": 16,
 *       "max_bandwidth_gbps": 63.01 },
 *     ...
 *   ]
 * }
 *
 * @param [in] jsonObj Pointer to the JSON result object
 */
void ListgpuTextPrinter::print(nlohmann::ordered_json *jsonObj)
{
	if (jsonObj->contains("error")) {
		PRINT("Error: {}\n", (*jsonObj)["error"].get<std::string>().c_str());
		return;
	}

	if (!jsonObj->contains("bdf_device_list")) {
		return;
	}

	TableBuilder table;

	std::vector<std::string> headers = {"PCI BDF Address", "PCI Slot", "PCIe Generation", "PCIe Max Link Width",
										"PCIe Max Bandwidth (GB/s)"};
	table.addColumn(headers[0], static_cast<int>(headers[0].size()))
		.addColumn(headers[1], static_cast<int>(headers[1].size() * 2)) // Slot labels can be long, give extra padding
		.addColumn(headers[2], static_cast<int>(headers[2].size()))
		.addColumn(headers[3], static_cast<int>(headers[3].size()))
		.addColumn(headers[4], static_cast<int>(headers[4].size()));

	for (auto &item : (*jsonObj)["bdf_device_list"]) {
		const std::string slot = item["pci_slot"].get<std::string>();
		const int gen = item["pcie_generation"].get<int>();
		const int width = item["max_link_width"].get<int>();
		const double bw = item["max_bandwidth_gbps"].get<double>();
		table.addRow(item["bdf"].get<std::string>(), slot.empty() ? "N/A" : slot, gen > 0 ? std::to_string(gen) : "N/A",
					 width > 0 ? std::to_string(width) : "N/A", bw > 0.0 ? std::format("{:.2f}", bw) : "N/A");
	}

	PRINT("{}", table.toString());
}

/**
 * @brief Runs the listpciinfo command
 *
 * Parses arguments, calls GET_XE_DEV_PCI_PROPS to enumerate all xe-bound
 * devices from sysfs, optionally filters by the requested BDF address, and
 * outputs the result as a table or JSON.
 *
 * @param [in] args Pointer to arg_struct containing argc/argv
 * @return 0 on success, non-zero on error
 */
int cmdListpciinfo::run(arg_struct *args)
{
	TRACING();

	// Reset state so the command is re-entrant if the instance is reused
	for (auto &[k, v] : listpciinfoCmds) {
		v.enabled = false;
		v.val.clear();
	}

	CLI::App sub{"Query PCI properties for xe-bound GPU(s)", "listpciinfo"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_option("-b,--bdf", listpciinfoCmds[listpciinfoCmdType::LISTPCIINFO_BDF].val,
				   "PCI BDF address to query (e.g. 0000:03:00.0)")
		->each([&](const std::string &) { listpciinfoCmds[listpciinfoCmdType::LISTPCIINFO_BDF].enabled = true; });
	sub.add_flag("-j,--json", listpciinfoCmds[listpciinfoCmdType::LISTPCIINFO_JSON].enabled,
				 "Print result in JSON format");

	try {
		sub.parse(args->argc - 1, args->argv + 1);
	} catch (const CLI::CallForHelp &) {
		help();
		return ZE_RESULT_SUCCESS;
	} catch (const CLI::ParseError &e) {
		ERR("{}", e.what());
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Discover all xe-bound devices and collect their PCI properties
	std::vector<xeDevPciInfo> pciPropsList;
	if (GET_XE_DEV_PCI_PROPS(&pciPropsList) != 0) {
		ERR("GET_XE_DEV_PCI_PROPS failed: cannot access /sys/bus/pci/drivers/xe\n");
		return 1;
	}

	auto jsonObj = std::make_unique<nlohmann::ordered_json>();
	nlohmann::ordered_json deviceArray = nlohmann::ordered_json::array();

	const std::string &filterBdf = listpciinfoCmds[listpciinfoCmdType::LISTPCIINFO_BDF].val;
	bool filtered = listpciinfoCmds[listpciinfoCmdType::LISTPCIINFO_BDF].enabled;
	bool found = false;

	for (const auto &info : pciPropsList) {
		if (filtered && info.bdf != filterBdf) {
			continue;
		}
		found = true;
		deviceArray.push_back({
			{"bdf", info.bdf},
			{"pci_slot", info.pciSlot},
			{"pcie_generation", info.pcieGeneration},
			{"max_link_width", info.maxLinkWidth},
			{"max_bandwidth_gbps", info.maxBandwidthGBps},
		});
	}

	if (filtered && !found) {
		(*jsonObj)["error"] = std::format("BDF '{}' not found among xe-bound devices", filterBdf);
	} else {
		(*jsonObj)["bdf_device_list"] = std::move(deviceArray);
	}

	std::unique_ptr<Printer> printer;
	if (listpciinfoCmds[listpciinfoCmdType::LISTPCIINFO_JSON].enabled) {
		printer = std::make_unique<JsonPrinter>();
	} else {
		printer = std::make_unique<ListgpuTextPrinter>();
	}

	printer->print(jsonObj.get());
	return 0;
}
