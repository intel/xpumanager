/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_ps.h"
#include "debug.h"
#include <CLI/CLI.hpp>
#include "table_builder.h"
#include <assert.h>
#include <sysprocess.h>

static std::unordered_map<psCmdType, psCmdStruct> psCmds = {
	{psCmdType::PS_HELP, {}},
	{psCmdType::PS_JSON, {}},
	{psCmdType::PS_DEVICE, {}},
};

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdPs::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "List status of processes"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s ps [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s ps", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s ps -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s ps -d [deviceId] -j", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "PID:      Process ID"));
	helpList.push_back(helpCmd(TITLE, "Command:  Process command name"));
	helpList.push_back(helpCmd(TITLE, "DeviceID: Device ID"));
	helpList.push_back(helpCmd(TITLE, "SHR:      The size of shared device memory mapped into this process (may not "
									  "necessarily be resident on the device at the time of reading) (kB)"));
	helpList.push_back(helpCmd(TITLE, "MEM:      Device memory size in bytes allocated by this process (may not "
									  "necessarily be resident on the device at the time of reading) (kB)"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 The device ID or PCI BDF address"));

	printHelp(helpList, helpType);
	helpList.clear();
}

PsTextPrinter::PsTextPrinter() : TextPrinter() {}

/**
 * @brief Prints device process information in a formatted text layout
 *
 * This function formats and prints a single device's process information from a JSON object.
 * It displays PID   Command   DeviceId   SHR    MEM columns
 *
 * @param jsonObj Pointer to the JSON object containing a single device's process information
 */
void PsTextPrinter::printDeviceInfo(nlohmann::ordered_json *jsonObj)
{
	if (jsonObj->contains("device_util_by_proc_list")) {
		TableBuilder table;
		table.addColumn("PID", 9)
			.addColumn("Command", 19)
			.addColumn("DeviceID", 14)
			.addColumn("SHR", 14)
			.addColumn("MEM", 14);

		for (auto &item : (*jsonObj)["device_util_by_proc_list"]) {
			table.addRow(item["process_id"].get<uint32_t>(), item["process_name"].get<std::string>(),
						 item["device_id"].get<uint32_t>(), item["shared_mem_size"].get<uint64_t>(),
						 item["mem_size"].get<uint64_t>());
		}

		PRINT("{}", table.toString().c_str());
	}
}

/**
 * @brief Calls printDeviceInfo function to print process information
 *
 * This function calls printDeviceInfo function based on the presence of device_list in json objects.
 *
 * @param jsonObj Pointer to the JSON object containing device's process information
 */
void PsTextPrinter::print(nlohmann::ordered_json *jsonObj)
{
	if (jsonObj->contains("error")) {
		PRINT("Error: {}\n", (*jsonObj)["error"].get<std::string>().c_str());
	} else {
		printDeviceInfo(jsonObj);
	}
}

/**
 * @brief Converts a psInfo structure into a JSON object
 *
 * This function serializes the psInfo structure into a JSON object.
 * It is used by nlohmann::json for automatic conversion via `to_json()`.
 *
 * @param Reference to a json object that will be filled with psInfo structure
 * @param Reference to a psInfo structure which has device process information
 */
// NOLINTNEXTLINE (readability-identifier-naming) // nlohmann_json requires to_json for ADL
void to_json(nlohmann::ordered_json &jsonObj, const psInfo &procInfo)
{
	// Clean process name by truncating at first null character
	std::string cleanProcessName = procInfo.commandName;
	size_t nullPos = cleanProcessName.find('\0');
	if (nullPos != std::string::npos) {
		cleanProcessName = cleanProcessName.substr(0, nullPos);
	}

	jsonObj = nlohmann::ordered_json{{"process_id", procInfo.processId},
									 {"process_name", cleanProcessName},
									 {"device_id", procInfo.devId},
									 {"engines", static_cast<uint64_t>(procInfo.engines)},
									 {"shared_mem_size", procInfo.sharedSize},
									 {"mem_size", procInfo.memSize}};
}

/**
 * @brief Gets the process information status for the given device
 *
 * This function retrieves the process information and populates the psInfoList vector.
 *
 * @param devInfo pointer
 * @param Reference value for psInfo structure
 * @return ze_result_t ZE_RESULT_SUCCESS if it retrieves process information
 */
ze_result_t cmdPs::getProcessList(const devInfo *dev, std::vector<psInfo> &psInfoList)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	std::vector<zes_process_state_t> processList;
	DBG("Running ps command on device {}\n", dev->index);
	process *ps = dev->dev->getProcess();
	if (ps == nullptr) {
		ERR("Error: Process pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}
	result = ps->getState(dev->zesDeviceHdl, &processList);
	for (auto &p : processList) {
		psInfoList.push_back(
			{p.processId, GETPROCESSNAME(p.processId), dev->index, p.engines, p.sharedSize / 1024, p.memSize / 1024});
	}
	return result;
}

/**
 * @brief Executes the ps run.
 *
 * @return int Returns 0 on success.
 */
int cmdPs::run(arg_struct *args)
{
	TRACING();
	ze_result_t result;
	std::vector<devInfo> deviceList;
	std::unique_ptr<Printer> printer;
	std::vector<zes_process_state_t> processList;

	// Reset state
	for (auto &[k, v] : psCmds) {
		v.enabled = false;
		v.val.clear();
	}

	CLI::App sub{"List running GPU processes", "ps"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", psCmds[psCmdType::PS_JSON].enabled, "Print result in JSON format");
	sub.add_option("-d,--device", psCmds[psCmdType::PS_DEVICE].val, "Device ID or PCI BDF address")
		->each([&](const std::string &) { psCmds[psCmdType::PS_DEVICE].enabled = true; });

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

	result = args->sm.findDevice(psCmds[psCmdType::PS_DEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '{}'.\n", psCmds[psCmdType::PS_DEVICE].val.c_str());
		return result;
	}

	std::vector<psInfo> psInfoList;
	auto jsonObj = std::make_unique<nlohmann::ordered_json>();

	if (!deviceList.empty()) {
		for (const auto &dev : deviceList) {
			result = getProcessList(&dev, psInfoList);
		}
		if (result != ZE_RESULT_SUCCESS) {
			DBG("Failed to get process information. Returned with error: {}\n", result);
			return result;
		}
		(*jsonObj)["device_util_by_proc_list"] = psInfoList;
	} else {
		(*jsonObj)["error"] = "device not found";
		(*jsonObj)["errno"] = ZE_RESULT_ERROR_UNINITIALIZED;
	}

	if (psCmds[psCmdType::PS_JSON].enabled == true) {
		printer = std::make_unique<JsonPrinter>();
	} else {
		printer = std::make_unique<PsTextPrinter>();
	}

	printer->print(jsonObj.get());

	return result;
}
