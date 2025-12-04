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

#include "cmd_ps.h"
#include "debug.h"
#include <assert.h>
#include <sysprocess.h>

static std::unordered_map<psCmdType, psCmdStruct> psCmds = {
	{psCmdType::PS_HELP, {{"help", no_argument, 0, 'h'}, nullptr, false, ""}},
	{psCmdType::PS_JSON, {{"json", no_argument, 0, 'j'}, nullptr, false, ""}},
	{psCmdType::PS_DEVICE, {{"device", required_argument, 0, 'd'}, nullptr, false, ""}},
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
	PRINT("PID       Command             DeviceID       SHR            MEM\n");
	for (auto &item : jsonObj->items()) {
		PRINT("%-9d %-19s %-14d %-14" PRIu64 " %-14" PRIu64 "\n", item.value()["process_id"].get<uint32_t>(),
			  item.value()["process_name"].get<std::string>().c_str(), item.value()["device_id"].get<uint32_t>(),
			  item.value()["shared_mem_size"].get<uint64_t>(), item.value()["mem_size"].get<uint64_t>());
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
	if (jsonObj->contains("device_list")) {
		for (auto &devJsonObj : (*jsonObj)["device_list"]) {
			printDeviceInfo(&devJsonObj);
		}
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
	jsonObj = nlohmann::ordered_json{{"process_id", procInfo.processId},
									 {"process_name", procInfo.commandName},
									 {"device_id", procInfo.devId},
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
	DBG("Running ps command on device %d\n", dev->index);
	process *ps = dev->dev->getProcess();
	if (ps == nullptr) {
		ERR("Error: Process pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}
	result = ps->getState(dev->zesDeviceHdl, &processList);
	for (auto &p : processList) {
		psInfoList.push_back(
			{p.processId, GETPROCESSNAME(p.processId), dev->index, p.sharedSize / 1024, p.memSize / 1024});
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
	int opt;
	int optionIndex = 0;
	std::string deviceId;
	std::unique_ptr<Printer> printer;
	std::vector<zes_process_state_t> processList;

	static struct option longOptions[] = {{"help", no_argument, 0, 'h'},
										  {"json", no_argument, 0, 'j'},
										  {"device", required_argument, 0, 'd'},
										  {0, 0, 0, 0}};

	// Skip the first two arguments (process and command name)
	int startind = 2;
	optind = 2;

	while ((opt = getopt_long(args->argc, args->argv, "hjd:", longOptions, &optionIndex)) != -1) {
		switch (opt) {
		case 'h':
			help();
			return ZE_RESULT_SUCCESS;
		case 'j':
			psCmds[psCmdType::PS_JSON].enabled = true;
			break;
		case 'd':
			psCmds[psCmdType::PS_DEVICE].enabled = true;
			psCmds[psCmdType::PS_DEVICE].val = optarg;
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

	result = args->sm.findDevice(psCmds[psCmdType::PS_DEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '%s'.\n", psCmds[psCmdType::PS_DEVICE].val.c_str());
		return result;
	}

	std::vector<psInfo> psInfoList;
	for (const auto &dev : deviceList) {
		result = getProcessList(&dev, psInfoList);
	}
	if (result != ZE_RESULT_SUCCESS) {
		DBG("Failed to get process information. Returned with error: %d\n", result);
		return result;
	}
	auto jsonObj = std::make_unique<nlohmann::ordered_json>(psInfoList);

	if (psCmds[psCmdType::PS_JSON].enabled == true) {
		printer = std::make_unique<JsonPrinter>();
	} else {
		printer = std::make_unique<PsTextPrinter>();
	}

	printer->print(jsonObj.get());

	return result;
}
