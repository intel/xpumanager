/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_diag.h"
#include "debug.h"
#include <CLI/CLI.hpp>
#include "table_builder.h"
#include <format>
#include <pci.h>
#include <power.h>
#include <chrono>
#include <thread>
#include <cmath>
#include <fstream>
#include <assert.h>
#include <ecc.h>
#include <fabric.h>
#include <frequency.h>
#include <scheduler.h>
#include <performance.h>
#include <standby.h>
#include <stdexcept>
#include <inttypes.h>
#include <ranges>
#include <charconv>
/*
 * @brief Command structure for diagnostic commands.
 * The way that this structure is defined allows for easy addition of new commands
 * by simply adding a new entry to the diagCmds map.
 * The command type is defined in the diagCmdType enum. It allows for easy
 * identification of the command type when requiring a specific command.
 * Next comes the option structure, which defines the command line options for the command.
 * The option structure is defined in the getopt.h header file. It allows for easy parsing
 * of command line options.
 * The next field is a pointer to the function that will be called when the command is executed.
 * This function is defined in the cmd_diag class. It doesn't have to be defined if a particular
 * command doesn't require a function to be called.
 * The next field is a boolean that indicates whether the command line option has been specified
 * by the user.
 * The last field is the value of the command line option (if any). This is simply stored as a
 * string. It is required to convert it to the appropriate type when the command is executed.
 */

static std::unordered_map<diagCmdType, diagCmdStruct> diagCmds = {
	{diagCmdType::DIAGHELP, {}},
	{diagCmdType::DIAGJSON, {}},
	{diagCmdType::DIAGDEVICE, {}},
	{diagCmdType::LEVEL, {.func = &cmdDiag::level}},
	{diagCmdType::PRECHECK, {}},
	{diagCmdType::STRESS, {.func = &cmdDiag::stress}},
	{diagCmdType::SINGLETEST, {.func = &cmdDiag::runSingleTest}},
	{diagCmdType::LISTTYPES, {}},
	{diagCmdType::GPU, {}},
	{diagCmdType::SINCE, {.func = &cmdDiag::runSince}},
	{diagCmdType::STRESSTIME, {}},
};

std::unordered_map<diagLevel, std::vector<std::pair<diagSubCmdType, diagSubLevelCmdStruct>>> cmdDiag::levelToDiagTests =
	{
		{diagLevel::LEVEL_1,
		 {
			 {
				 DIAG_LEVEL_SW_LIBRARY,
				 {&cmdDiag::checkLibrary, "XPUM_DIAG_SOFTWARE_LIBRARY", true},
			 },
			 {
				 DIAG_LEVEL_SW_PERMISSION,
				 {&cmdDiag::checkAccessPermission, "XPUM_DIAG_SOFTWARE_PERMISSION", true},
			 },
			 {
				 DIAG_LEVEL_SW_EXCLUSIVE,
				 {&cmdDiag::checkExclusive, "XPUM_DIAG_SOFTWARE_EXCLUSIVE", true},
			 },
			 {
				 DIAG_LEVEL_COMPUTATIONFUNCTEST,
				 {&cmdDiag::computationFuncTest, "XPUM_DIAG_LIGHT_COMPUTATION", true},
			 },
		 }},
		{diagLevel::LEVEL_2,
		 {
			 {
				 DIAG_LEVEL_PCIEBANDWIDTH,
				 {&cmdDiag::pcieBandwidth, "XPUM_DIAG_INTEGRATION_PCIE", true},
			 },
			 {
				 DIAG_LEVEL_MEDIA,
				 {&cmdDiag::mediaFuncTest, "XPUM_DIAG_MEDIA_CODEC", true},
			 },
		 }},
		{diagLevel::LEVEL_3,
		 {
			 {
				 DIAG_LEVEL_COMPUTATION,
				 {&cmdDiag::computation, "XPUM_DIAG_PERFORMANCE_COMPUTATION", true},
			 },
			 {
				 DIAG_LEVEL_POWER,
				 {&cmdDiag::powerTest, "XPUM_DIAG_PERFORMANCE_POWER", true},
			 },
			 {
				 DIAG_LEVEL_MEMORYBANDWIDTH,
				 {&cmdDiag::memoryBandwidth, "XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH", true},
			 },
			 {
				 DIAG_LEVEL_MEMORYALLOCATION,
				 {&cmdDiag::memoryAllocation, "XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION", true},
			 },
			 {
				 DIAG_LEVEL_MEMORYERROR,
				 {&cmdDiag::memoryError, "XPUM_DIAG_MEMORY_ERROR", true},
			 },
		 }},
};

/**
 * @brief Wraps text to fit within specified width, returning lines.
 *
 * Splits text at word boundaries to fit within maxWidth. Continuation
 * lines are indented by 2 spaces.
 *
 * @param[in] text Text to wrap.
 * @param[in] maxWidth Maximum width per line.
 * @return Vector of wrapped lines.
 */
static std::vector<std::string> wrapText(std::string_view text, size_t maxWidth)
{
	constexpr size_t indentSize = 2;
	constexpr size_t minWidth = indentSize + 1;
	std::vector<std::string> lines;

	if (maxWidth < minWidth) {
		maxWidth = minWidth;
	}

	if (text.length() <= maxWidth) {
		lines.emplace_back(text);
		return lines;
	}

	std::string_view remaining = text;
	bool firstLine = true;

	while (!remaining.empty()) {
		const size_t lineWidth = firstLine ? maxWidth : (maxWidth - indentSize);

		if (remaining.length() <= lineWidth) {
			lines.push_back(std::format("{}{}", firstLine ? "" : "  ", remaining));
			break;
		}

		size_t breakPos = remaining.rfind(' ', lineWidth);
		if (breakPos == std::string_view::npos || breakPos == 0) {
			breakPos = lineWidth;
		}

		auto line = remaining.substr(0, breakPos);
		if (auto lastNonSpace = line.find_last_not_of(' '); lastNonSpace != std::string_view::npos) {
			line = line.substr(0, lastNonSpace + 1);
		}
		lines.push_back(std::format("{}{}", firstLine ? "" : "  ", line));

		remaining = remaining.substr(breakPos);
		if (auto firstNonSpace = remaining.find_first_not_of(' '); firstNonSpace != std::string_view::npos) {
			remaining = remaining.substr(firstNonSpace);
		} else {
			remaining = {};
		}

		firstLine = false;
	}

	return lines;
}

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdDiag::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Run some test suites to diagnose GPU"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s diag [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [deviceId] -l [level]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [pciBdfAddress] -l [level]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [deviceId] -l [level] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [pciBdfAddress] -l [level] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [deviceId] --singletest [testIds]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [pciBdfAddress] --singletest [testIds]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [deviceId] --singletest [testIds] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [pciBdfAddress] --singletest [testIds] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [deviceIds] --stress", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag -d [deviceIds] --stress --stresstime [time]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck --listtypes", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck --listtypes -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck --gpu", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck --gpu -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck --since [startTime]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck --since [startTime] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck --gpu --since [startTime]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --precheck --gpu --since [startTime] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --stress", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s diag --stress --stresstime [time]", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 The device ID or PCI BDF address"));
	helpList.push_back(
		helpCmd(HEADING, "-l,--level                  The diagnostic levels to run. The valid options include"));
	helpList.push_back(helpCmd(SUB_HEADING2, "1. quick test"));
	helpList.push_back(helpCmd(
		SUB_HEADING2,
		"2. medium test - this diagnostic level will have the significant performance impact on the specified GPUs"));
	helpList.push_back(helpCmd(
		SUB_HEADING2,
		"3. long test - this diagnostic level will have the significant performance impact on the specified GPUs"));
	helpList.push_back(helpCmd(HEADING, "-s,--stress                 Stress the GPU(s) for the specified time"));
	helpList.push_back(helpCmd(HEADING, "--stresstime                Stress time (in minutes)"));
	helpList.push_back(helpCmd(HEADING, "--precheck                  Do the precheck on the GPU and GPU driver. By "
										"default, precheck scans kernel messages by journalctl"));
	helpList.push_back(helpCmd(SUB_HEADING, "It could be configured to scan dmesg or log file through xpum.conf"));
	helpList.push_back(helpCmd(HEADING, "--listtypes                 List all supported GPU error types"));
	helpList.push_back(helpCmd(HEADING, "--gpu                       Show the GPU status only"));
	helpList.push_back(helpCmd(HEADING, "--since                     Start time for log scanning. It only works with "
										"the journalctl option. The generic format is \"YYYY-MM-DD HH:MM:SS\""));
	helpList.push_back(helpCmd(SUB_HEADING, "Alternatively the strings \"yesterday\", \"today\" are also understood"));
	helpList.push_back(helpCmd(
		SUB_HEADING,
		"Relative times also may be specified, prefixed with \"-\" referring to times before the current time"));
	helpList.push_back(helpCmd(SUB_HEADING, "Scanning would start from the latest boot if it is not specified"));
	helpList.push_back(
		helpCmd(HEADING, "--singletest                Selectively run some particular tests. Separated by the comma"));
	helpList.push_back(helpCmd(SUB_HEADING2, "1. Computation"));
	helpList.push_back(helpCmd(SUB_HEADING2, "2. Memory Error"));
	helpList.push_back(helpCmd(SUB_HEADING2, "3. Memory Bandwidth"));
	helpList.push_back(helpCmd(SUB_HEADING2, "4. Media Codec"));
	helpList.push_back(helpCmd(SUB_HEADING2, "5. PCIe Bandwidth"));
	helpList.push_back(helpCmd(SUB_HEADING2, "6. Power"));
	helpList.push_back(helpCmd(SUB_HEADING2, "7. Computation functional test"));
	helpList.push_back(helpCmd(SUB_HEADING2, "8. Media Codec functional test"));
	helpList.push_back(helpCmd(SUB_HEADING2, "9. Memory Allocation"));
	helpList.push_back(helpCmd(SUB_HEADING, "Note that in a multi NUMA node server, it may need to use numactl to "
											"specify which node the PCIe bandwidth test runs on"));
	helpList.push_back(helpCmd(
		SUB_HEADING,
		"Usage: numactl [ --membind nodes ] [ --cpunodebind nodes ] xpu-smi diag -d [deviceId] --singletest 5"));
	helpList.push_back(helpCmd(SUB_HEADING, "It also applies to diag level tests"));

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Prints Precheck information.
 *
 * @param[in] deviceList A list of device information structures.
 * @param[in] gpuOnly A flag of GPU display.
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success.
 */
ze_result_t cmdDiag::printPrecheckInfo(std::vector<devInfo> deviceList, std::unique_ptr<Printer> &printer, bool gpuOnly)
{
	TRACING();
	std::string outputLine;
	std::string resultLine;
	std::string fwStatus;

	if (gpuOnly && !diagCmds[diagCmdType::PRECHECK].enabled) {
		ERR("--gpu requires --precheck.\n");
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	auto componentListJson = std::make_unique<nlohmann::ordered_json>();
	outputLine = (driverLoaded) ? std::string("Pass") : std::string("Fail");
	auto componentJson = printPreCheckDetail(std::string(), std::string(), outputLine, std::string("Driver"));
	componentListJson->push_back(*componentJson);

	if (!gpuOnly) {
		auto componentCPUJson =
			printPreCheckDetail(std::string("0"), std::string(), CHECKCPUSTATUS(), std::string("CPU"));
		componentListJson->push_back(*componentCPUJson);
	}

	for (auto &device : deviceList) {
		pci *p = device.dev->getPCI();
		if (p == nullptr) {
			ERR("Failed to get PCI device properties.\n");
			return ZE_RESULT_ERROR_UNKNOWN;
		}
		outputLine = p->getBDFStr();
		fwStatus = p->getFWStatus();
		resultLine = ((fwStatus == std::string("normal")) && CHECKPCIELINKSTATUS(outputLine)) ? std::string("Pass")
																							  : std::string("Unknown");
		auto componentGPUJson = printPreCheckDetail(std::string(), outputLine, resultLine, std::string("GPU"));
		componentListJson->push_back(*componentGPUJson);
	}

	auto devicesJson = std::make_unique<nlohmann::ordered_json>();
	(*devicesJson)["component_list"] = *componentListJson;

	printer->print(devicesJson.get());

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes stress testing diagnostics on the device
 *
 * This function runs intensive stress tests designed to validate device
 * stability and performance under heavy computational loads. It helps
 * identify potential hardware issues and thermal limitations.
 *
 * @param[in] d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if stress tests pass, error code otherwise
 */
ze_result_t cmdDiag::stress(devInfo *d, UNUSED nlohmann::ordered_json *jsonObj)
{
	TRACING();
	ze_result_t res;
	const std::string filename = "ze_int_compute.spv";
	const std::string moduleName = "compute_int_v1";
	int inputValue = 4;
	long double current;
	uint64_t round = 0;
	bool featureOnly;
	std::string outputLine;

	if (diagCmds[diagCmdType::STRESSTIME].enabled) {
		if (stoi(diagCmds[diagCmdType::STRESSTIME].val) < 0) {
			ERR("Stress time should be greater than or equal to 0.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	diagnostic *diag = d->dev->getDiagnostic();
	if (diag == nullptr) {
		ERR("Device does not support diagnostics.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	PRINT("Started to stress GPU and would update the status in every minute \n");
	featureOnly = false;
	outputLine = std::to_string(d->index);
	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds(60));
		round++;
		res = diag->computationTest(d, 2048, &inputValue, sizeof(int), filename, moduleName, current, featureOnly);
		if (res != ZE_RESULT_SUCCESS) {
			ERR("Failed to run stress: {}\n", l0_error_to_string(res));
			return res;
		} else {
			PRINT("Stress on GPU: {}; Round {}; Integer compute: {:f} GIOPS.\n", outputLine.c_str(), round, current);
			if (diagCmds[diagCmdType::STRESSTIME].enabled) {
				if ((stoi(diagCmds[diagCmdType::STRESSTIME].val) != 0) &&
					(round >= static_cast<uint64_t>(stoi(diagCmds[diagCmdType::STRESSTIME].val)))) {
					PRINT("Finish stressing.\n");
					return ZE_RESULT_SUCCESS;
				}
			}
		}
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief check Library.
 *
 * This function check Library on a device to ensure the system in the healthy states.
 * It loads the device related info and performs the tests:
 * 1. Retrieves the device's PCI properties
 * 2. Make sure firmware get loaded and work in normal state.
 * 3. Make sure all the driver & related library get loaded properly.
 *
 * @param[in] d Pointer to device information structure
 * @return ze_result_t ZE_RESULT_SUCCESS on success, or an error code on failure
 */
ze_result_t cmdDiag::checkLibrary(devInfo *d, UNUSED nlohmann::ordered_json *jsonObj)
{
	std::string fwStatus;

	pci *p = d->dev->getPCI();
	if (p == nullptr) {
		ERR("Failed to get PCI device properties.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	fwStatus = p->getFWStatus();
	if ((fwStatus != "normal") || !(driverLoaded)) {
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief check access permission.
 *
 * This function check permission on a device to ensure having necessary access permission.
 * It performs the tests:
 * 1. check if there is a /dev/dri folder and can accessed or not
 * 2. check if there is any entry name under the /dev/dri, which has the prefix with "renderD"
 *    and after are all digits and can be accessed or not
 *
 * @param[in] d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on success, or an error code on failure
 */
ze_result_t cmdDiag::checkAccessPermission(UNUSED devInfo *d, UNUSED nlohmann::ordered_json *jsonObj)
{
	if (!CHECKPERMISSION()) {
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief check exclusive.
 *
 * This function check if the host processes can simultaneously & exclusively run on the specified device.
 * It performs the tests:
 * 1. loads the number of processes connected to the device and run them dynamically,
 * 2. check if the processes currently attached to the device can run successfully.
 * 3. check if all these process related stream files, which have names with "/proc/" + std::to_string(processId)
 *    + "/cmdline" have healthy states and ready for i/o operations.
 *
 * @param[in] d Pointer to device information structure
 * @return ze_result_t ZE_RESULT_SUCCESS on success, or an error code on failure
 */
ze_result_t cmdDiag::checkExclusive(devInfo *d, UNUSED nlohmann::ordered_json *jsonObj)
{
	ze_result_t ret;
	std::vector<zes_process_state_t> processList;

	process *ps = (process *)d->dev->getProcess();
	if (ps == nullptr) {
		ERR("Error: Process pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	ret = ps->getState(d->zesDeviceHdl, &processList);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Failed to get process states\n");
		return ret;
	}

	for (const auto &proc : processList) {
		if (!CHECKPROCESSEXCLUSIVE(proc.processId)) {
			return ZE_RESULT_ERROR_UNKNOWN;
		}
	}

	return ZE_RESULT_SUCCESS;
}

std::unique_ptr<nlohmann::ordered_json> cmdDiag::printDiagDetail(std::string componentType, std::string componentMsg,
																 std::string componentResult, bool finished)
{
	auto jsonObj = std::make_unique<nlohmann::ordered_json>();

	// Get string values first, then assign to JSON
	(*jsonObj)["component_type"] = componentType;
	(*jsonObj)["finished"] = finished;
	(*jsonObj)["message"] = componentMsg;
	(*jsonObj)["result"] = componentResult;

	return jsonObj;
}

/**
 * @brief Prints detailed precheck component information in JSON format
 *
 * @param[in] preId string for the CPU id info
 * @param[in] preBdf string for the GPU BDF info
 * @param[in] preStatus string for the component status info
 * @param[in] preType string for the component type info
 * @return std::unique_ptr<nlohmann::ordered_json> JSON object containing device details
 */
std::unique_ptr<nlohmann::ordered_json> cmdDiag::printPreCheckDetail(std::string preId, std::string preBdf,
																	 std::string preStatus, std::string preType)
{
	auto jsonObj = std::make_unique<nlohmann::ordered_json>();

	// Get string values first, then assign to JSON
	if (preId != "") {
		(*jsonObj)["id"] = preId;
	}
	if (preBdf != "") {
		(*jsonObj)["bdf"] = preBdf;
	}
	if (preStatus != "") {
		(*jsonObj)["status"] = preStatus;
	}
	if (preType != "") {
		(*jsonObj)["type"] = preType;
	}
	return jsonObj;
}

/**
 * @brief prints detailed precheck error component information in JSON format
 *
 * @param[in] errCategory string for the error Category info
 * @param[in] errID string for the error id info
 * @param[in] errSeverity string for the error Severity info
 * @param[in] errType string for the error type info
 * @return std::unique_ptr<nlohmann::ordered_json> JSON object containing error component details
 */
std::unique_ptr<nlohmann::ordered_json> cmdDiag::printErrorDetail(std::string errCategory, std::string errID,
																  std::string errSeverity, std::string errType)
{
	auto jsonObj = std::make_unique<nlohmann::ordered_json>();

	// Get string values first, then assign to JSON
	(*jsonObj)["error_category"] = errCategory;
	(*jsonObj)["error_id"] = errID;
	(*jsonObj)["error_severity"] = errSeverity;
	(*jsonObj)["error_type"] = errType;

	return jsonObj;
}

/**
 * @brief prints precheck error information in JSON format
 *
 * @param[in][out] errListJson nlohmann::ordered_json for the error info
 * @param[in] errCategory string for the error Category info
 * @param[in] errID string for the error id info
 * @param[in] errSeverity string for the error Severity info
 * @param[in] preType string for the error type info
 */
void cmdDiag::printErrorInfo(nlohmann::ordered_json *errListJson, std::string errCategory, std::string errID,
							 std::string errSeverity, std::string errType)
{
	auto errJson = printErrorDetail(errCategory, errID, errSeverity, errType);
	errListJson->push_back(*errJson);
}

/**
 * @brief prints single diagnostic test information in JSON format
 * @param[in] d pointer to device information structure
 * @param[in] jsonObj nlohmann::ordered_json pointer
 * @param[in] type string for the test type info
 * @param[in] msg string for the test message info
 * @param[in] result string for the test result info
 */
void cmdDiag::printSingleDiagInfo(devInfo *d, nlohmann::ordered_json *jsonObj, std::string type, std::string msg,
								  std::string result)
{
	// Set device_id if not already set
	if (!jsonObj->contains("device_id")) {
		(*jsonObj)["device_id"] = d->index;
	}

	// Initialize component_list if not already present
	if (!jsonObj->contains("component_list")) {
		(*jsonObj)["component_list"] = nlohmann::ordered_json::array();
	}

	// Append new component
	bool finished = (result == "Pass" || result == "Fail");
	auto componentJson = printDiagDetail(type, msg, result, finished);
	(*jsonObj)["component_list"].push_back(*componentJson);
}

/**
 * @brief Validates and processes diagnostic level parameter
 *
 * This function validates the diagnostic level parameter provided by the user,
 * ensuring it falls within the valid range (1-3). Each level represents different
 * test intensity: 1 for quick tests, 2 for medium tests, 3 for comprehensive tests.
 *
 * @param[in] d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if level is valid, error code otherwise
 */
ze_result_t cmdDiag::level(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	bool finalResult = true;
	int totalCount = 0;
	std::string msgLine, resultLine;
	// Check if the level is valid
	if (diagCmds[diagCmdType::LEVEL].val.empty()) {
		ERR("Level is required.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Convert the level string to an integer
	int level = stoi(diagCmds[diagCmdType::LEVEL].val);
	if (level < 1 || level > 3) {
		ERR("Invalid level. Valid options are 1, 2, or 3.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	auto startTime = std::chrono::system_clock::now();
	auto startTimeS = std::chrono::floor<std::chrono::seconds>(startTime);
	auto startTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(startTime - startTimeS);
	std::string startTimeStr = std::format("{:%Y-%m-%dT%H:%M:%S}.{:03d}", startTimeS, startTimeMs.count());

	for (int le = 1; le <= level; le++) {
		for (auto &test : levelToDiagTests.at(static_cast<diagLevel>(le))) {

			DBG("Preparing to run test for level {}\n", le);
			result = (this->*test.second.func)(d, jsonObj);
			if (result != ZE_RESULT_SUCCESS) {
				finalResult = false;
				test.second.result = false;
			}
			totalCount++;
		}
	}

	auto endTime = std::chrono::system_clock::now();
	auto endTimeS = std::chrono::floor<std::chrono::seconds>(endTime);
	auto endTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - endTimeS);
	std::string endTimeStr = std::format("{:%Y-%m-%dT%H:%M:%S}.{:03d}", endTimeS, endTimeMs.count());

	auto componentListJson = std::make_unique<nlohmann::ordered_json>();
	for (int le = 1; le <= level; le++) {
		for (const auto &test : levelToDiagTests.at(static_cast<diagLevel>(le))) {
			DBG("Preparing to run test for level {}\n", le);
			bool testFinished = test.second.result;
			resultLine = testFinished ? "Pass" : "Fail";

			// Create user-friendly messages based on test type
			const auto userFriendlyName = test.second.showString;
			if (userFriendlyName == "XPUM_DIAG_SOFTWARE_LIBRARY") {
				msgLine = testFinished ? "Pass to check libraries." : "Fail to check libraries.";
			} else if (userFriendlyName == "XPUM_DIAG_SOFTWARE_PERMISSION") {
				msgLine = testFinished ? "Pass to check permission." : "Fail to check permission.";
			} else if (userFriendlyName == "XPUM_DIAG_SOFTWARE_EXCLUSIVE") {
				msgLine =
					testFinished ? "Pass to check the software exclusive." : "Fail to check the software exclusive.";
			} else if (userFriendlyName == "XPUM_DIAG_LIGHT_COMPUTATION") {
				msgLine = testFinished ? "Pass to check computation functionality."
									   : "Fail to check computation functionality.";
			} else {
				msgLine = std::format("{} to check {}.", testFinished ? "Pass" : "Fail", userFriendlyName);
			}

			auto componentJson = printDiagDetail(userFriendlyName, msgLine, resultLine, testFinished);
			componentListJson->push_back(*componentJson);
		}
	}

	(*jsonObj)["component_count"] = totalCount;
	(*jsonObj)["component_list"] = *componentListJson;
	(*jsonObj)["device_id"] = d->index;
	(*jsonObj)["end_time"] = endTimeStr;
	(*jsonObj)["finished"] = finalResult;
	(*jsonObj)["level"] = level;
	(*jsonObj)["message"] = finalResult ? "All diagnostics done" : "Some diagnostics failed";
	(*jsonObj)["result"] = (finalResult) ? "Pass" : "Fail";
	(*jsonObj)["start_time"] = startTimeStr;

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes a specific diagnostic test based on user selection
 *
 * This function runs individual diagnostic tests selected by the user through
 * the --singletest parameter. It maps test IDs to corresponding test functions
 * and executes the requested diagnostic test on the specified device.
 *
 * @param[in] d Pointer to device information structure containing device context
 * @return ze_result_t ZE_RESULT_SUCCESS if test completes successfully, error code otherwise
 */
ze_result_t cmdDiag::runSingleTest(devInfo *d, nlohmann::ordered_json *jsonObj)

{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;

	// Map user-facing test numbers to enum values (matching help text order)
	static std::unordered_map<int, diagSubSingleCmdType> testIdMap = {
		{1, DIAG_SINGLE_COMPUTATION},		  {2, DIAG_SINGLE_MEMORYERROR},
		{3, DIAG_SINGLE_MEMORYBANDWIDTH},	  {4, DIAG_SINGLE_MEDIA},
		{5, DIAG_SINGLE_PCIEBANDWIDTH},		  {6, DIAG_SINGLE_POWER},
		{7, DIAG_SINGLE_COMPUTATIONFUNCTEST}, {8, DIAG_SINGLE_MEDIAFUNCTEST},
		{9, DIAG_SINGLE_MEMORYALLOCATION},
	};

	static std::unordered_map<diagSubSingleCmdType, diagSubSingleCmdStruct> diagSingleTests = {
		{DIAG_SINGLE_COMPUTATION, {&cmdDiag::computation}},
		{DIAG_SINGLE_MEMORYERROR, {&cmdDiag::memoryError}},
		{DIAG_SINGLE_MEMORYBANDWIDTH, {&cmdDiag::memoryBandwidth}},
		{DIAG_SINGLE_MEDIA, {&cmdDiag::mediaCodec}},
		{DIAG_SINGLE_PCIEBANDWIDTH, {&cmdDiag::pcieBandwidth}},
		{DIAG_SINGLE_POWER, {&cmdDiag::powerTest}},
		{DIAG_SINGLE_COMPUTATIONFUNCTEST, {&cmdDiag::computationFuncTest}},
		{DIAG_SINGLE_MEDIAFUNCTEST, {&cmdDiag::mediaFuncTest}},
		{DIAG_SINGLE_MEMORYALLOCATION, {&cmdDiag::memoryAllocation}},
	};

	// Parse comma-separated test IDs
	const std::string_view testIds = diagCmds[diagCmdType::SINGLETEST].val;
	std::vector<int> testList;

	for (const auto token : testIds | std::views::split(',')) {
		const std::string_view tokenView(token.begin(), token.end());
		int testId = 0;
		const auto [ptr, ec] = std::from_chars(tokenView.data(), tokenView.data() + tokenView.size(), testId);
		if (ec != std::errc{}) {
			ERR("Invalid test ID: '{}'\n", tokenView);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		testList.push_back(testId);
	}

	auto startTime = std::chrono::system_clock::now();
	auto startTimeS = std::chrono::floor<std::chrono::seconds>(startTime);
	auto startTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(startTime - startTimeS);
	std::string startTimeStr = std::format("{:%Y-%m-%dT%H:%M:%S}.{:03d}", startTimeS, startTimeMs.count());

	// Set up device_id once if not already set
	if (!jsonObj->contains("device_id")) {
		(*jsonObj)["device_id"] = d->index;
		(*jsonObj)["component_list"] = nlohmann::ordered_json::array();
	}

	// Run each test and track if any fail
	bool anyFailed = false;
	for (const int testId : testList) {
		if (const auto testTypeIt = testIdMap.find(testId); testTypeIt != testIdMap.end()) {
			if (const auto testIt = diagSingleTests.find(testTypeIt->second); testIt != diagSingleTests.end()) {
				INFO("Running test: {}\n", testId);
				const ze_result_t testResult = (this->*testIt->second.func)(d, jsonObj);
				if (testResult != ZE_RESULT_SUCCESS) {
					anyFailed = true;
				}
			}
		} else {
			ERR("Invalid test ID: {}. Valid test IDs are 1-9.\n", testId);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	// Return generic error if any test failed; specific errors are in JSON
	if (anyFailed) {
		result = ZE_RESULT_ERROR_UNKNOWN;
	}

	const auto endTime = std::chrono::system_clock::now();
	const auto endTimeS = std::chrono::floor<std::chrono::seconds>(endTime);
	const auto endTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - endTimeS);
	const std::string endTimeStr = std::format("{:%Y-%m-%dT%H:%M:%S}.{:03d}", endTimeS, endTimeMs.count());

	// Add summary fields to match expected format
	const bool allPassed = (result == ZE_RESULT_SUCCESS);
	(*jsonObj)["component_count"] = static_cast<int>(testList.size());
	(*jsonObj)["device_id"] = d->index;
	(*jsonObj)["end_time"] = endTimeStr;
	(*jsonObj)["errno"] = static_cast<int>(result);
	(*jsonObj)["finished"] = true;
	(*jsonObj)["message"] = allPassed ? "All diagnostics done" : "Some diagnostics failed";
	(*jsonObj)["result"] = allPassed ? "Pass" : "Fail";
	(*jsonObj)["start_time"] = startTimeStr;

	return result;
}

/**
 * @brief Runs the computation diagnostic test on the specified device when user runs diag --singletest 1
 *
 * This function performs a floating-point computation test on the GPU device to measure its performance
 * in terms of GFLOPS. It loads a SPIR-V kernel, allocates necessary device memory, sets up and launches
 * the kernel, and reports the measured performance.
 *
 * @param[in] d Pointer to the device information structure.
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success, or an appropriate error code on failure.
 */
ze_result_t cmdDiag::computation(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();

	const std::string fileName = "ze_sp_compute.spv";
	const std::string moduleName = "compute_sp_v1";
	float inputValue = 1.3f;
	bool featureOnly;
	long double current;
	std::string msg;

	// Call into HAL to perform computation test
	diagnostic *diag = d->dev->getDiagnostic();
	if (diag == nullptr) {
		ERR("Device does not support diagnostics.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	featureOnly = false;
	ze_result_t res =
		diag->computationTest(d, 4096, &inputValue, sizeof(float), fileName, moduleName, current, featureOnly);
	if (diagCmds[diagCmdType::SINGLETEST].enabled) {
		if (res != ZE_RESULT_SUCCESS) {
			msg = std::format("Fail to check computation. Test failed:{}", l0_error_to_string(res));
			printSingleDiagInfo(d, jsonObj, "Computation", msg, "Fail");
		} else {
			msg = "Pass to check computation.";
			if (!featureOnly) {
				msg = msg + std::format(" Computation test completed with {:.3Lf} GFLOPS. ", current);
			}
			printSingleDiagInfo(d, jsonObj, "Computation", msg, "Pass");
		}
	}
	return res;
}

/**
 * @brief Executes memory error diagnostic test
 *
 * This function performs memory error detection tests on the GPU device,
 * checking for memory corruption, ECC errors, and memory integrity issues.
 * It validates GPU memory subsystem reliability and health.
 *
 * @param[in] d Pointer to device information structure
 * @return ze_result_t ZE_RESULT_SUCCESS if memory tests pass, error code otherwise
 */
ze_result_t cmdDiag::memoryError(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();

	int errorCount;
	std::string msg;

	pci *p = d->dev->getPCI();
	if (p == nullptr) {
		ERR("Failed to get PCI device properties.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	if (p->getFuncType() == devFuncType::DEVICE_FUNCTION_TYPE_VIRTUAL) {
		ERR("Device does not support this diagnostic feature.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	diagnostic *diag = d->dev->getDiagnostic();
	if (diag == nullptr) {
		ERR("Device does not support diagnostics.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t res = diag->memoryErrorTest(d, errorCount);
	if (diagCmds[diagCmdType::SINGLETEST].enabled) {
		if (res != ZE_RESULT_SUCCESS) {
			msg = std::format("Fail to test memory error.Test failed:{}", l0_error_to_string(res));
			if (errorCount > 0)
				msg = msg + std::format("Error count: {}.", errorCount);
			printSingleDiagInfo(d, jsonObj, "Memory error", msg, "Fail");

		} else {
			msg = "Pass to test memory error.";
			printSingleDiagInfo(d, jsonObj, "Memory error", msg, "Pass");
		}
	}
	return res;
}

/**
 * @brief Executes memory allocation diagnostic test
 *
 * This function measures GPU memory allocation performance by running
 * memory-intensive kernels and calculating throughput metrics. It validates
 * memory subsystem performance and identifies potential bandwidth limitations.
 *
 * @param[in] d Pointer to device information structure
 * @return ze_result_t ZE_RESULT_SUCCESS if allocation tests complete, error code otherwise
 */
ze_result_t cmdDiag::memoryAllocation(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();
	std::string msg;

	pci *p = d->dev->getPCI();
	if (p == nullptr) {
		ERR("Failed to get PCI device properties.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	if (p->getFuncType() == devFuncType::DEVICE_FUNCTION_TYPE_VIRTUAL) {
		ERR("Device does not support this diagnostic feature.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	diagnostic *diag = d->dev->getDiagnostic();
	if (diag == nullptr) {
		ERR("Device does not support diagnostics.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t res = diag->peformanceMemoryAllocation(d);
	if (diagCmds[diagCmdType::SINGLETEST].enabled) {
		if (res != ZE_RESULT_SUCCESS) {
			msg = std::format("Fail to check memory allocation. Test failed:{}", l0_error_to_string(res));
			printSingleDiagInfo(d, jsonObj, "Memory allocation", msg, "Fail");
		} else {
			msg = "Pass to check memory allocation.";
			printSingleDiagInfo(d, jsonObj, "Memory allocation", msg, "Pass");
		}
	}
	return res;
}

/**
 * @brief Executes memory bandwidth diagnostic test
 *
 * This function measures GPU memory bandwidth performance by running
 * memory-intensive kernels and calculating throughput metrics. It validates
 * memory subsystem performance and identifies potential bandwidth limitations.
 *
 * @param[in] d Pointer to device information structure
 * @return ze_result_t ZE_RESULT_SUCCESS if bandwidth tests complete, error code otherwise
 */
ze_result_t cmdDiag::memoryBandwidth(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();

	std::vector<long double> totalGbps;
	std::string msg;

	pci *p = d->dev->getPCI();
	if (p == nullptr) {
		ERR("Failed to get PCI device properties.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	if (p->getFuncType() == devFuncType::DEVICE_FUNCTION_TYPE_VIRTUAL) {
		ERR("Device does not support this diagnostic feature.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	diagnostic *diag = d->dev->getDiagnostic();
	if (diag == nullptr) {
		ERR("Device does not support diagnostics.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t res = diag->memoryBandwidthTest(d, totalGbps);
	if (diagCmds[diagCmdType::SINGLETEST].enabled) {
		if (res != ZE_RESULT_SUCCESS) {
			msg = std::format("Fail to check memory bandwidth. Test failed:{}", l0_error_to_string(res));
			printSingleDiagInfo(d, jsonObj, "Memory bandwidth", msg, "Fail");
		} else {
			msg = "Pass to test memory bandwidth.";
			for (std::size_t i = 0; i < totalGbps.size(); i++) {
				msg = msg + std::format(" Memory bandwidth on copy engine group {} is {:.3Lf} GBPS.", i, totalGbps[i]);
			}
			printSingleDiagInfo(d, jsonObj, "Memory bandwidth", msg, "Pass");
		}
	}
	return res;
}

/**
 * @brief Executes media codec diagnostic test
 *
 * This function tests GPU media encoding and decoding capabilities,
 * validating media engine functionality and performance. It checks
 * video codec acceleration and media processing capabilities.
 *
 * @param[in] d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if media tests pass, error code otherwise
 */
ze_result_t cmdDiag::mediaCodec(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();

	std::string bdfStr;
	std::string outputLine;
	std::string msg;

	pci *p = d->dev->getPCI();
	if (p == nullptr) {
		ERR("Failed to get PCI device properties.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	bdfStr = p->getBDFStr();
	if (!CHECKMEDIACODEC(bdfStr, true, outputLine)) {
		if (diagCmds[diagCmdType::SINGLETEST].enabled) {
			msg = std::format("Fail to check media transcode. Test failed:{}", outputLine);
			printSingleDiagInfo(d, jsonObj, "Media transcode", msg, "Fail");
		}
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	if (diagCmds[diagCmdType::SINGLETEST].enabled) {
		msg = std::format("Pass to check media transcode.{}", outputLine);
		printSingleDiagInfo(d, jsonObj, "Media transcode", msg, "Pass");
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes PCIe bandwidth diagnostic test
 *
 * This function measures PCIe interface bandwidth between CPU and GPU,
 * testing data transfer rates and identifying potential PCIe bottlenecks.
 * It validates system interconnect performance and PCIe link integrity.
 *
 * @param[in] d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if PCIe tests complete, error code otherwise
 */
ze_result_t cmdDiag::pcieBandwidth(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();

	std::string bdfStr;
	std::string outputLine;
	long double totalBandwidth;
	long double totalLatency;
	std::string msg;

	pci *p = d->dev->getPCI();
	if (p == nullptr) {
		ERR("Failed to get PCI device properties.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	bdfStr = p->getBDFStr();
	diagnostic *diag = d->dev->getDiagnostic();
	if (diag == nullptr) {
		ERR("Device does not support diagnostics.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t res = diag->pcieBandwidthTest(d, totalBandwidth, totalLatency);
	if (diagCmds[diagCmdType::SINGLETEST].enabled) {
		if (res != ZE_RESULT_SUCCESS) {
			msg = std::format("Fail to check PCIe bandwidth. Test failed:{}", l0_error_to_string(res));
			printSingleDiagInfo(d, jsonObj, "PCIe bandwidth", msg, "Fail");
		} else {
			msg = "Pass to check PCIe bandwidth. ";
			msg = msg + std::format("Total pcie bandwidth is: {} GB/s. Total pcie latency is: {} us.", totalBandwidth,
									totalLatency);
			if (p->getFuncType() != devFuncType::DEVICE_FUNCTION_TYPE_VIRTUAL) {
				outputLine = GETDOWNGRADEDPCIEINFO(bdfStr);
				if (outputLine.size() != 0) {
					msg = msg + outputLine;
				}
			}
			printSingleDiagInfo(d, jsonObj, "PCIe bandwidth", msg, "Pass");
		}
	}
	return res;
}

/**
 * @brief Executes power management diagnostic test
 *
 * This function tests GPU power management capabilities on the specified device
 * It performs intensive computation calculations, then measures the power.
 *
 * The test:
 * 1. Set up the intensive computation
 * 2. perform this intensive computation
 * 3. Measure and calculate the power
 *
 * @param[in] d Pointer to the device information structure containing the device handle and context
 * @return ze_result_t ZE_RESULT_SUCCESS on successful test completion, or an error code on failure
 */
ze_result_t cmdDiag::powerTest(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();

	ze_result_t ret;
	const std::string filename = "ze_int_compute.spv";
	const std::string moduleName = "compute_int_v1";
	int inputValue = 4;
	bool featureOnly;
	long double current;
	int totalPowerValue;
	std::string msg;

	diagnostic *diag = d->dev->getDiagnostic();
	if (diag == nullptr) {
		ERR("Device does not support diagnostics.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	featureOnly = false;
	ret = diag->computationTest(d, 2048, &inputValue, sizeof(int), filename, moduleName, current, featureOnly);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Computation test failed: {}\n", l0_error_to_string(ret));
		return ret;
	}

	ret = diag->calculatePowerConsumption(d, totalPowerValue);
	if (diagCmds[diagCmdType::SINGLETEST].enabled) {
		if (ret != ZE_RESULT_SUCCESS) {
			msg = std::format("Fail to check power test. Test failed:{}", l0_error_to_string(ret));
			printSingleDiagInfo(d, jsonObj, "Power test", msg, "Fail");
		} else {
			msg = "Pass to check power test.";
			if (!featureOnly) {
				msg = msg + std::format("Computation test completed with {} GFLOPS. ", current);
			}
			msg = msg + std::format(" Update peak power value: {} w.", totalPowerValue);
			printSingleDiagInfo(d, jsonObj, "Power test", msg, "Pass");
		}
	}
	return ret;
}

/**
 * @brief Executes computation functional diagnostic test
 *
 * This function performs functional validation of GPU compute capabilities,
 * running basic compute operations to verify correct GPU functionality
 * without performance measurement emphasis.
 *
 * @param[in] d Pointer to device information structure
 * @return ze_result_t ZE_RESULT_SUCCESS if functional tests pass, error code otherwise
 */
ze_result_t cmdDiag::computationFuncTest(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();

	const std::string fileName = "ze_sp_compute.spv";
	const std::string moduleName = "compute_sp_v1";
	float inputValue = 1.3f;
	bool featureOnly;
	long double current;
	std::string msg;

	diagnostic *diag = d->dev->getDiagnostic();
	if (diag == nullptr) {
		ERR("Device does not support diagnostics.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	featureOnly = true;
	ze_result_t res =
		diag->computationTest(d, 4096, &inputValue, sizeof(float), fileName, moduleName, current, featureOnly);
	if (diagCmds[diagCmdType::SINGLETEST].enabled) {
		if (res != ZE_RESULT_SUCCESS) {
			msg = std::format("Fail to check computation. Test failed:{}", l0_error_to_string(res));
			printSingleDiagInfo(d, jsonObj, "Computation feature", msg, "Fail");

		} else {
			msg = "Pass to check computation feature.";
			if (!featureOnly) {
				msg = msg + std::format(" Computation test completed with {} GFLOPS. ", current);
			}
			printSingleDiagInfo(d, jsonObj, "Computation feature", msg, "Pass");
		}
	}
	return res;
}

/**
 * @brief Executes media functional diagnostic test
 *
 * This function performs functional validation of GPU media engines,
 * testing basic media processing capabilities to verify correct media
 * subsystem operation without performance measurement emphasis.
 *
 * @param[in] d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if media functional tests pass, error code otherwise
 */
ze_result_t cmdDiag::mediaFuncTest(UNUSED devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();

	std::string pdfStr;
	std::string outputLine;
	std::string msg;

	pci *p = d->dev->getPCI();
	if (p == nullptr) {
		ERR("Failed to get PCI device properties.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	pdfStr = p->getBDFStr();
	if (!CHECKMEDIACODEC(pdfStr, true, outputLine)) {
		if (diagCmds[diagCmdType::SINGLETEST].enabled) {
			msg = std::format("Fail to check media transcode. Test failed:{}", outputLine);
			printSingleDiagInfo(d, jsonObj, "Media transcode feature", msg, "Fail");
		}
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	if (diagCmds[diagCmdType::SINGLETEST].enabled) {
		msg = std::format("Pass to check media transcode.{}", outputLine);
		printSingleDiagInfo(d, jsonObj, "Media transcode feature", msg, "Pass");
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints formatted table borders for diagnostic output
 *
 * This utility function prints formatted table borders used in diagnostic
 * output displays, providing consistent visual formatting for tabular
 * diagnostic results and error type listings.
 */
ze_result_t cmdDiag::printListTypes(std::unique_ptr<Printer> &printer)
{
	TRACING();

	if (!diagCmds[diagCmdType::PRECHECK].enabled) {
		ERR("--listtypes requires --precheck.\n");
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	auto errListJson = std::make_unique<nlohmann::ordered_json>();
	printErrorInfo(errListJson.get(), "Hardware", "1", "Critical", "GuC Not Running");
	printErrorInfo(errListJson.get(), "Hardware", "2", "Critical", "GuC Error");
	printErrorInfo(errListJson.get(), "Hardware", "3", "Critical", "GuC Initialization Failed");
	printErrorInfo(errListJson.get(), "Hardware", "4", "Critical", "IOMMU Catastrophic Error");
	printErrorInfo(errListJson.get(), "Hardware", "5", "Critical", "LMEM Not Initialized By Firmware");
	printErrorInfo(errListJson.get(), "Hardware", "6", "Critical", "PCIe Error");
	printErrorInfo(errListJson.get(), "Kernel Mode Driver", "7", "Critical", "DRM Error");
	printErrorInfo(errListJson.get(), "Kernel Mode Driver", "8", "Critical", "GPU Hang");
	printErrorInfo(errListJson.get(), "Kernel Mode Driver", "9", "Critical", "Xe Error");
	printErrorInfo(errListJson.get(), "Kernel Mode Driver", "10", "Critical", "Xe Not Loaded");
	printErrorInfo(errListJson.get(), "Kernel Mode Driver", "11", "Critical", "Level Zero Init Error");
	printErrorInfo(errListJson.get(), "Hardware", "12", "High", "HuC Disabled");
	printErrorInfo(errListJson.get(), "Hardware", "13", "High", "HuC Not Running");
	printErrorInfo(errListJson.get(), "User Mode Driver", "14", "High", "Level Zero Metrics Init Error");
	printErrorInfo(errListJson.get(), "Hardware", "15", "Critical", "Memory Error");
	printErrorInfo(errListJson.get(), "Hardware", "16", "Critical", "GPU Initialization Failed");
	printErrorInfo(errListJson.get(), "Kernel Mode Driver", "17", "High", "MEI Error");

	auto errsJson = std::make_unique<nlohmann::ordered_json>();
	(*errsJson)["error_type_list"] = *errListJson;
	printer->print(errsJson.get());

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief get key display name for an input key
 *
 * This utility function provides key display name for an input key used in diagnostic
 * output displays
 * @param[in] key reference to std::string &key
 */
std::string getKeyDisplayName(const std::string &key)
{
	static const std::unordered_map<std::string, std::string> keyDisplayMap = {
		{"device_id", "Device ID"},
		{"level", "Level"},
		{"result", "Result"},
		{"component_count", "Items"},
		{"XPUM_DIAG_SOFTWARE_LIBRARY", "Software Library"},
		{"XPUM_DIAG_SOFTWARE_PERMISSION", "Software Permission"},
		{"XPUM_DIAG_SOFTWARE_EXCLUSIVE", "Software Exclusive"},
		{"XPUM_DIAG_LIGHT_COMPUTATION", "Computation Check"},
		{"XPUM_DIAG_INTEGRATION_PCIE", "PCIe Bandwidth"},
		{"XPUM_DIAG_MEDIA_CODEC", "Media Codec"},
		{"XPUM_DIAG_PERFORMANCE_COMPUTATION", "Computation"},
		{"XPUM_DIAG_PERFORMANCE_POWER", "Power"},
		{"XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH", "Memory Bandwidth"},
		{"XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION", "Memory Allocation"},
		{"XPUM_DIAG_MEMORY_ERROR", "Memory Error"},
	};

	const auto it = keyDisplayMap.find(key);
	if (it != keyDisplayMap.end()) {
		return it->second;
	}
	return key;
}

/**
 * @brief Prints test format for diagnostic output
 *
 * This utility function prints formatted text info used in diagnostic
 * output displays
 * @param[in] jsonObj Pointer to nlohmann::ordered_json object structure
 */
void DiagTextPrinter::print(nlohmann::ordered_json *jsonObj)
{
	TRACING();

	auto valueToString = [](const nlohmann::ordered_json &val) -> std::string {
		if (val.is_string()) {
			return val.get<std::string>();
		}
		return val.dump();
	};

	// Calculate required column widths based on content
	constexpr int totalWidth = 100;	  // Total table width
	constexpr int borderOverhead = 5; // Borders and padding (| + space on each side + |)
	constexpr int minCol2Width = 20;  // Minimum second column width
	int maxCol1Width = 15;			  // Minimum first column width

	if (jsonObj->contains("level")) {
		static const std::vector<std::string> headerFields = {"device_id", "level", "result", "component_count"};
		for (const auto &field : headerFields) {
			if (jsonObj->contains(field)) {
				maxCol1Width = std::max(maxCol1Width, static_cast<int>(getKeyDisplayName(field).length()));
			}
		}
		for (const auto &component : (*jsonObj)["component_list"]) {
			const std::string componentType = valueToString(component["component_type"]);
			maxCol1Width = std::max(maxCol1Width, static_cast<int>(getKeyDisplayName(componentType).length()));
		}

		const int col1 = maxCol1Width;
		const int col2 = std::max(minCol2Width, totalWidth - maxCol1Width - borderOverhead);

		TableBuilder table;
		table.addColumn(getKeyDisplayName("device_id"), col1);
		table.addColumn(jsonObj->contains("device_id") ? valueToString((*jsonObj)["device_id"]) : "", col2);

		// Print remaining header fields (device_id is already the column header)
		for (const auto &field : headerFields) {
			if (field != "device_id" && jsonObj->contains(field)) {
				table.addRow(getKeyDisplayName(field), valueToString((*jsonObj)[field]));
			}
		}

		table.addSeparator(BorderStyle::Normal);

		// Print components with word-wrapped messages
		const auto &componentList = (*jsonObj)["component_list"];
		for (size_t i = 0; i < componentList.size(); ++i) {
			const auto &component = componentList[i];
			const std::string componentType = valueToString(component["component_type"]);
			const std::string friendlyName = getKeyDisplayName(componentType);
			table.addRow(friendlyName, std::format("Result: {}", valueToString(component["result"])));
			const std::string messageInfo = std::format("Message: {}", valueToString(component["message"]));
			const auto wrappedLines = wrapText(messageInfo, static_cast<size_t>(col2));
			for (const auto &line : wrappedLines) {
				table.addRow("", line);
			}
			if (i + 1 < componentList.size()) {
				table.addSeparator(BorderStyle::Normal);
			}
		}

		PRINT("{}", table.toString().c_str());
	} else if (jsonObj->contains("device_id")) {
		maxCol1Width = std::max(maxCol1Width, static_cast<int>(getKeyDisplayName("device_id").length()));
		for (const auto &component : (*jsonObj)["component_list"]) {
			const std::string componentType = valueToString(component["component_type"]);
			maxCol1Width = std::max(maxCol1Width, static_cast<int>(getKeyDisplayName(componentType).length()));
		}

		const int col1 = maxCol1Width;
		const int col2 = std::max(minCol2Width, totalWidth - maxCol1Width - borderOverhead);

		TableBuilder table;
		table.addColumn(getKeyDisplayName("device_id"), col1);
		table.addColumn(valueToString((*jsonObj)["device_id"]), col2);

		const auto &componentList = (*jsonObj)["component_list"];
		for (size_t i = 0; i < componentList.size(); ++i) {
			const auto &component = componentList[i];
			const std::string componentType = valueToString(component["component_type"]);
			const std::string friendlyName = getKeyDisplayName(componentType);
			table.addRow(friendlyName, std::format("Result: {}", valueToString(component["result"])));
			const std::string messageInfo = std::format("Message: {}", valueToString(component["message"]));
			const auto wrappedLines = wrapText(messageInfo, static_cast<size_t>(col2));
			for (const auto &line : wrappedLines) {
				table.addRow("", line);
			}
			if (i + 1 < componentList.size()) {
				table.addSeparator(BorderStyle::Normal);
			}
		}

		PRINT("{}", table.toString().c_str());
	} else if (jsonObj->contains("error_type_list")) {
		TableBuilder table;
		table.addColumn("Error ID", 10);
		table.addColumn("Error Type", 35);
		table.addColumn("Error Category", 25);
		table.addColumn("Error Severity", 21);

		for (const auto &component : (*jsonObj)["error_type_list"]) {
			table.addRow(valueToString(component["error_id"]), valueToString(component["error_type"]),
						 valueToString(component["error_category"]), valueToString(component["error_severity"]));
		}

		PRINT("{}", table.toString().c_str());
	} else if (jsonObj->contains("component_list")) {
		const int col2 = std::max(minCol2Width, totalWidth - maxCol1Width - 25);

		TableBuilder table;
		table.addColumn("Component", maxCol1Width);
		table.addColumn("Details", col2);

		const auto &componentList = (*jsonObj)["component_list"];
		for (size_t i = 0; i < componentList.size(); ++i) {
			const auto &component = componentList[i];
			if (component["type"].get<std::string>() == "Driver") {
				table.addRow(component["type"].get<std::string>(),
							 std::format("Status: {}", component["status"].get<std::string>()));
			} else if (component["type"].get<std::string>() == "CPU") {
				table.addRow(component["type"].get<std::string>(),
							 std::format("CPU ID: {}", component["id"].get<std::string>()));
				table.addRow("", std::format("Status: {}", component["status"].get<std::string>()));
			} else if (component["type"].get<std::string>() == "GPU") {
				table.addRow(component["type"].get<std::string>(),
							 std::format("BDF: {}", component["bdf"].get<std::string>()));
				table.addRow("", std::format("Status: {}", component["status"].get<std::string>()));
			}
			if (i + 1 < componentList.size()) {
				table.addSeparator(BorderStyle::Normal);
			}
		}

		PRINT("{}", table.toString().c_str());
	}
}

/**
 * @brief Executes time-based log scanning for diagnostic precheck
 *
 * This function scans system logs starting from a specified time point
 * to identify GPU-related errors and issues. It provides historical
 * system analysis for diagnostic precheck operations.
 *
 * @param[in] d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if log scanning completes, error code otherwise
 */
ze_result_t cmdDiag::runSince(UNUSED devInfo *d, UNUSED nlohmann::ordered_json *jsonObj)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the diag run.
 *
 * @return int Returns 0 on success.
 */
int cmdDiag::run(arg_struct *args)
{
	TRACING();
	std::vector<devInfo> deviceList;
	ze_result_t result;
	std::unique_ptr<Printer> printer;

	// Reset state
	for (auto &[k, v] : diagCmds) {
		v.enabled = false;
		v.val.clear();
	}

	CLI::App sub{"Run GPU diagnostics", "diag"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", diagCmds[diagCmdType::DIAGJSON].enabled, "Print result in JSON format");
	sub.add_option("-d,--device", diagCmds[diagCmdType::DIAGDEVICE].val, "Device ID or PCI BDF address")
		->each([&](const std::string &) { diagCmds[diagCmdType::DIAGDEVICE].enabled = true; });
	sub.add_option("-l,--level", diagCmds[diagCmdType::LEVEL].val, "Diagnostic level (1-3)")
		->each([&](const std::string &) { diagCmds[diagCmdType::LEVEL].enabled = true; });
	sub.add_flag("--precheck", diagCmds[diagCmdType::PRECHECK].enabled, "Run pre-check diagnostics");
	sub.add_flag("-s,--stress", diagCmds[diagCmdType::STRESS].enabled, "Run stress diagnostics");
	sub.add_option("--singletest", diagCmds[diagCmdType::SINGLETEST].val, "Run a single diagnostic test")
		->each([&](const std::string &) { diagCmds[diagCmdType::SINGLETEST].enabled = true; });
	sub.add_flag("--listtypes", diagCmds[diagCmdType::LISTTYPES].enabled, "List all diagnostic test types");
	sub.add_flag("--gpu", diagCmds[diagCmdType::GPU].enabled, "Run GPU diagnostics only");
	sub.add_option("--since", diagCmds[diagCmdType::SINCE].val, "Run diagnostics since a given timestamp")
		->each([&](const std::string &) { diagCmds[diagCmdType::SINCE].enabled = true; });
	sub.add_option("--stresstime", diagCmds[diagCmdType::STRESSTIME].val, "Stress test duration (seconds)")
		->each([&](const std::string &) { diagCmds[diagCmdType::STRESSTIME].enabled = true; });

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

	// Check if any actual diagnostic command was specified
	bool hasCommand = false;
	for (const auto &cmd : diagCmds) {
		// Skip help and json flags - they're modifiers, not commands
		if (cmd.first != diagCmdType::DIAGHELP && cmd.first != diagCmdType::DIAGJSON) {
			if (cmd.second.enabled) {
				hasCommand = true;
				break;
			}
		}
	}

	// If no command was specified, show help
	if (!hasCommand) {
		help();
		return 0;
	}

	if (diagCmds[diagCmdType::DIAGJSON].enabled == true) {
		printer = std::make_unique<JsonPrinter>();
	} else {
		printer = std::make_unique<DiagTextPrinter>();
	}

	result = args->sm.findDevice(diagCmds[diagCmdType::DIAGDEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '{}'.\n", diagCmds[diagCmdType::DIAGDEVICE].val.c_str());
		return result;
	}

	driverLoaded = args->sm.isDriverLoaded();
	if (diagCmds[diagCmdType::LISTTYPES].enabled) {
		printListTypes(printer);
	} else if (diagCmds[diagCmdType::GPU].enabled) {
		printPrecheckInfo(deviceList, printer, true);
	} else if (diagCmds[diagCmdType::PRECHECK].enabled && !diagCmds[diagCmdType::GPU].enabled &&
			   !diagCmds[diagCmdType::LISTTYPES].enabled) {
		printPrecheckInfo(deviceList, printer, false);
	} else {
		INFO("Running diagnostics can take several minutes to complete.\n");
		// Iterate through the device list and execute the command
		auto jsonObj = std::make_unique<nlohmann::ordered_json>();
		// JSON object to collect diagnostic results for all devices and commands
		for (auto &device : deviceList) {
			if (device.dev->isIGPU()) {
				DBG("Diagnostics are only supported on discrete GPUs so skipping device {}.\n", device.index);
				continue;
			}

			// Call the appropriate command function based on the command type
			for (const auto &cmd : diagCmds) {
				if (cmd.second.enabled && cmd.second.func != nullptr) {
					DBG("Running command: {}\n", diagCmdName(cmd.first));
					result = (this->*cmd.second.func)(&device, jsonObj.get());
					if (diagCmds[diagCmdType::DIAGDEVICE].enabled) {
						DBG("Exiting command: {}\n", diagCmdName(cmd.first));
						break;
					}
				}
			}
		}

		printer->print(jsonObj.get());
	}
	return result;
}
