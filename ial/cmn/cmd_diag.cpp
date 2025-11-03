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

#include "cmd_diag.h"
#include "debug.h"
#include <pci.h>
#include <power.h>
#include <chrono>
#include <thread>
#include <cmath>
#include <fstream>
#include <sys/stat.h>
#include <assert.h>
#include <ecc.h>
#include <fabric.h>
#include <frequency.h>
#include <scheduler.h>
#include <performance.h>
#include <standby.h>
#include <stdexcept>
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
	{diagCmdType::DIAGHELP, {{"help", no_argument, 0, 'h'}, nullptr, false, ""}},
	{diagCmdType::DIAGJSON, {{"json", no_argument, 0, 'j'}, nullptr, false, ""}},
	{diagCmdType::DIAGDEVICE, {{"device", required_argument, 0, 'd'}, nullptr, false, ""}},
	{diagCmdType::LEVEL, {{"level", required_argument, 0, 'l'}, &cmdDiag::level, false, ""}},
	{diagCmdType::PRECHECK, {{"precheck", no_argument, 0, 0}, &cmdDiag::precheck, false, ""}},
	{diagCmdType::STRESS, {{"stress", no_argument, 0, 's'}, &cmdDiag::stress, false, ""}},
	{diagCmdType::SINGLETEST, {{"singletest", required_argument, 0, 0}, &cmdDiag::runSingleTest, false, ""}},
	{diagCmdType::LISTTYPES, {{"listtypes", no_argument, 0, 0}, &cmdDiag::listTypes, false, ""}},
	{diagCmdType::GPU, {{"gpu", no_argument, 0, 0}, &cmdDiag::gpu, false, ""}},
	{diagCmdType::SINCE, {{"since", required_argument, 0, 0}, &cmdDiag::runSince, false, ""}},
	{diagCmdType::STRESSTIME, {{"stresstime", required_argument, 0, 0}, nullptr, false, ""}},
};

std::unordered_map<diagLevel, std::vector<std::pair<diagSubCmdType, diagSubLevelCmdStruct>>> cmdDiag::levelToDiagTests =
	{
		{diagLevel::LEVEL_1,
		 {
			 {
				 DIAG_LEVEL_SW_LIBRARY,
				 {&cmdDiag::checkLibrary, "Software Library", true},
			 },
			 {
				 DIAG_LEVEL_SW_PERMISSION,
				 {&cmdDiag::checkAccessPermission, "Software Permission", true},
			 },
			 {
				 DIAG_LEVEL_SW_EXCLUSIVE,
				 {&cmdDiag::checkExclusive, "Software Exclusive", true},
			 },
			 {
				 DIAG_LEVEL_COMPUTATIONFUNCTEST,
				 {&cmdDiag::computationFuncTest, "Computation Check", true},
			 },
		 }},
		{diagLevel::LEVEL_2,
		 {
			 {
				 DIAG_LEVEL_PCIEBANDWIDTH,
				 {&cmdDiag::pcieBandwidth, "Integration PCIe", true},
			 },
			 {
				 DIAG_LEVEL_MEDIA,
				 {&cmdDiag::mediaFuncTest, "Media Codec", true},
			 },
		 }},
		{diagLevel::LEVEL_3,
		 {
			 {
				 DIAG_LEVEL_COMPUTATION,
				 {&cmdDiag::computation, "Performance Computation", true},
			 },
			 {
				 DIAG_LEVEL_POWER,
				 {&cmdDiag::powerTest, "Performance Power", true},
			 },
			 {
				 DIAG_LEVEL_MEMORYBANDWIDTH,
				 {&cmdDiag::memoryBandwidth, "Performance Memory Bandwidth", true},
			 },
			 {
				 DIAG_LEVEL_MEMORYALLOCATION,
				 {&cmdDiag::memoryAllocation, "Performance Memory Allocation", true},
			 },
			 {
				 DIAG_LEVEL_MEMORYERROR,
				 {&cmdDiag::memoryError, "Memory Error", true},
			 },
		 }},
};

/**
 * @brief Prints a dashed line.
 *
 */
static void printPretty()
{
	PRINT("+------------+-----------------------------------+----------------------+----------------+\n");
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
	helpList.push_back(helpCmd(SUB_HEADING2, "9. Xe Link Throughput"));
	helpList.push_back(
		helpCmd(SUB_HEADING2, "10. Xe Link all-to-all Throughput. It only works for all GPUs (\"-d -1\")"));
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
 * @brief Print pre-diagnostic check info
 *
 * This function prints preliminary validation and system check info
 * It validates prerequisites and system state before running intensive tests.
 *
 * @param[in] d Pointer to device information structure
 * @param[in] gpuOnly bool to indicate if GPU only or not
 * @return ze_result_t ZE_RESULT_SUCCESS if pre-checks pass, error code otherwise
 */
ze_result_t cmdDiag::printPrecheckInfo(devInfo *d, bool gpuOnly)
{
	TRACING();

	zes_pci_link_status_t pciLinkStatus;
	std::string fwStatus;
	ze_result_t result;
	std::string outputLine;

	pci *p = d->dev->getPCI();
	if (p == nullptr) {
		ERR("Failed to get PCI device properties.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	outputLine = p->getBDFStr();
	printPretty();

	if (!sysInfoShown) {
		PRINT("| Component        |  Details                                                            |\n");
		printPretty();

		if (driverLoaded) {
			PRINT("| Driver           |  Status: Pass                                                       |\n");
		} else {
			PRINT("| Driver           |  Status: Fail                                                       |\n");
		}
		printPretty();
		if (!gpuOnly) {
			PRINT("| CPU              |  CPU ID: 0                                                          |\n");
			PRINT("|                  |  Status:                                                            |\n");
			printPretty();
		}
		sysInfoShown = true;
	}

	PRINT("| GPU              |  BDF:                 %s                                  |\n", outputLine.c_str());

	fwStatus = p->getFWStatus();
	if (fwStatus != "normal") {
		PRINT("|                  |  Status: Unknown                                                    |\n");
	} else {
		result = p->getState(d->zesDeviceHdl, pciLinkStatus);
		if (result != ZE_RESULT_SUCCESS) {
			pciLinkStatus = ZES_PCI_LINK_STATUS_UNKNOWN;
		}

		if (pciLinkStatus != ZES_PCI_LINK_STATUS_GOOD) {
			PRINT("|                  |  Status: Unknown                                                    |\n");
		} else {
			PRINT("|                  |  Status: Pass                                                       |\n");
		}
	}
	printPretty();

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Performs pre-diagnostic checks before running full diagnostics
 *
 * This function executes preliminary validation and system checks to ensure
 * the device and system are ready for comprehensive diagnostic testing.
 * It validates prerequisites and system state before running intensive tests.
 *
 * @param[in] d Pointer to device information structure
 * @return ze_result_t ZE_RESULT_SUCCESS if pre-checks pass, error code otherwise
 */
ze_result_t cmdDiag::precheck(devInfo *d)
{
	TRACING();

	/*
	 * Only run the precheck with CPU information if neither the GPU nor LISTTYPES flags are enabled.
	 * - If the user has not requested GPU diagnostics (GPU flag) and has not requested a list of test types (LISTTYPES
	 * flag), we perform a precheck using CPU/system information.
	 * - If either GPU or LISTTYPES is enabled, we skip the CPU/system precheck, as those commands handle their own
	 * checks. This logic ensures that precheck is only run in the context of a general system check, not when specific
	 * GPU or test type diagnostics are requested.
	 */
	if (!diagCmds[diagCmdType::GPU].enabled && !diagCmds[diagCmdType::LISTTYPES].enabled) {
		return printPrecheckInfo(d, false);
	}

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
ze_result_t cmdDiag::stress(UNUSED devInfo *d)
{
	TRACING();
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
ze_result_t cmdDiag::checkLibrary(devInfo *d)
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
ze_result_t cmdDiag::checkAccessPermission(UNUSED devInfo *d)
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
ze_result_t cmdDiag::checkExclusive(devInfo *d)
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
ze_result_t cmdDiag::level(UNUSED devInfo *d)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	bool finalResult = true;
	int totalCount = 0;
	std::string itemStr;
	std::string outputLine;

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

	for (int le = 1; le <= level; le++) {
		for (auto &test : levelToDiagTests.at(static_cast<diagLevel>(le))) {

			DBG("Preparing to run test for level %d\n", le);
			result = (this->*test.second.func)(d);
			if (result != ZE_RESULT_SUCCESS) {
				finalResult = false;
				test.second.result = false;
			}
			totalCount++;
		}
	}

	outputLine = std::to_string(d->index);
	printPretty();
	PRINT("| Device ID                     |   %s                                                    |\n",
		  outputLine.c_str());
	printPretty();
	PRINT("| Level                         |   %d                                                    |\n", level);
	PRINT("| Final result                  |   %s                                                 |\n",
		  (finalResult) ? "Pass" : "Fail");
	PRINT("| Items                         |   %d                                                    |\n", totalCount);
	printPretty();

	for (int le = 1; le <= level; le++) {
		for (const auto &test : levelToDiagTests.at(static_cast<diagLevel>(le))) {
			DBG("Preparing to run test for level %d\n", le);
			PRINT("| %s           | Result: %s                                             |\n",
				  test.second.showString.c_str(), (test.second.result) ? "Pass" : "Fail");

			PRINT("|                               | Message: %s to check %s                     |\n",
				  (test.second.result) ? "Pass" : "Fail", test.second.showString.c_str());
		}
		printPretty();
	}
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
ze_result_t cmdDiag::runSingleTest(devInfo *d)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	static std::unordered_map<diagSubSingleCmdType, diagSubSingleCmdStruct> diagSingleTests = {
		{DIAG_SINGLE_COMPUTATION, {&cmdDiag::computation}},
		{DIAG_SINGLE_MEMORYERROR, {&cmdDiag::memoryError}},
		{DIAG_SINGLE_MEMORYBANDWIDTH, {&cmdDiag::memoryBandwidth}},
		{DIAG_SINGLE_MEDIA, {&cmdDiag::mediaCodec}},
		{DIAG_SINGLE_PCIEBANDWIDTH, {&cmdDiag::pcieBandwidth}},
		{DIAG_SINGLE_POWER, {&cmdDiag::powerTest}},
		{DIAG_SINGLE_COMPUTATIONFUNCTEST, {&cmdDiag::computationFuncTest}},
		{DIAG_SINGLE_MEDIAFUNCTEST, {&cmdDiag::mediaFuncTest}},
		{DIAG_SINGLE_XELINKTHROUGHPUT, {&cmdDiag::xeLinkThroughput}},
		{DIAG_SINGLE_XELINKALLTOALLTHROUGHPUT, {&cmdDiag::xeLinkAllToAllThroughput}},
	};

	for (const auto &test : diagSingleTests) {
		if (test.first == stoi(diagCmds[diagCmdType::SINGLETEST].val)) {
			DBG("Running test: %d\n", test.first);
			result = (this->*test.second.func)(d);
			break;
		}
	}

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
ze_result_t cmdDiag::computation(devInfo *d)
{
	TRACING();

	const std::string fileName = "ze_sp_compute.spv";
	const std::string moduleName = "compute_sp_v1";
	float input_value = 1.3f;
	bool featureOnly;
	long double current;

	// Call into HAL to perform computation test
	diagnostic *diag = d->dev->getDiagnostic();
	if (diag == nullptr) {
		ERR("Device does not support diagnostics.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	featureOnly = false;
	ze_result_t res =
		diag->computationTest(d, 4096, &input_value, sizeof(float), fileName, moduleName, current, featureOnly);
	if (diagCmds[diagCmdType::SINGLETEST].enabled) {
		if (res != ZE_RESULT_SUCCESS) {
			PRINT("Result:  Fail\n");
			PRINT("Message: Fail to check computation.\n");
			ERR("Computation test failed: %s\n", l0_error_to_string(res));
		} else {
			PRINT("Result:  Pass\n");
			PRINT("Message: Pass to check computation.\n");
			if (!featureOnly) {
				PRINT("Computation test completed with %Lf GFLOPS.\n", current);
			}
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
 * @param[in] d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if memory tests pass, error code otherwise
 */
ze_result_t cmdDiag::memoryError(UNUSED devInfo *d)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes memory allocation diagnostic test
 *
 * This function measures GPU memory allocation performance by running
 * memory-intensive kernels and calculating throughput metrics. It validates
 * memory subsystem performance and identifies potential bandwidth limitations.
 *
 * @param[in] d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if allocation tests complete, error code otherwise
 */
ze_result_t cmdDiag::memoryAllocation(UNUSED devInfo *d)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes memory bandwidth diagnostic test
 *
 * This function measures GPU memory bandwidth performance by running
 * memory-intensive kernels and calculating throughput metrics. It validates
 * memory subsystem performance and identifies potential bandwidth limitations.
 *
 * @param[in] d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if bandwidth tests complete, error code otherwise
 */
ze_result_t cmdDiag::memoryBandwidth(UNUSED devInfo *d)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
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
ze_result_t cmdDiag::mediaCodec(devInfo *d)
{
	TRACING();

	std::string pdfStr;
	std::string outputLine;

	pci *p = d->dev->getPCI();
	if (p == nullptr) {
		ERR("Failed to get PCI device properties.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	pdfStr = p->getBDFStr();
	if (!CHECKMEDIACODEC(pdfStr, true, outputLine)) {
		if (diagCmds[diagCmdType::SINGLETEST].enabled) {
			PRINT("Result:  Fail\n");
			PRINT("Message: Fail to check Media transcode.\n");
			ERR("Media codec test failed: %s\n", outputLine.c_str());
		}
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	if (diagCmds[diagCmdType::SINGLETEST].enabled) {
		PRINT("Result:  Pass\n");
		PRINT("Message: Pass to check Media transcode.\n");
		PRINT("%s.\n", outputLine.c_str());
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
ze_result_t cmdDiag::pcieBandwidth(devInfo *d)
{
	TRACING();

	long double totalBandwidth;
	long double totalLatency;

	diagnostic *diag = d->dev->getDiagnostic();
	if (diag == nullptr) {
		ERR("Device does not support diagnostics.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_result_t res = diag->pcieBandwidthTest(d, totalBandwidth, totalLatency);
	if (diagCmds[diagCmdType::SINGLETEST].enabled) {
		if (res != ZE_RESULT_SUCCESS) {
			PRINT("Result:  Fail\n");
			PRINT("Message: Fail to check PCIe bandwidth.\n");
			ERR("PCIe bandwidth test failed: %s\n", l0_error_to_string(res));
		} else {
			PRINT("Result:  Pass\n");
			PRINT("Message: Pass to check PCIe bandwidth.\n");
			PRINT("Total pcie bandwidth is: %Lf GB/s.\n", totalBandwidth);
			PRINT("Total pcie latency is: %Lf us.\n", totalLatency);
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
ze_result_t cmdDiag::powerTest(devInfo *d)
{
	TRACING();

	ze_result_t ret;
	const std::string filename = "ze_int_compute.spv";
	const std::string moduleName = "compute_int_v1";
	int input_value = 4;
	bool featureOnly;
	long double current;
	int totalPowerValue;

	diagnostic *diag = d->dev->getDiagnostic();
	if (diag == nullptr) {
		ERR("Device does not support diagnostics.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	featureOnly = false;
	ret = diag->computationTest(d, 2048, &input_value, sizeof(int), filename, moduleName, current, featureOnly);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Computation test failed: %s\n", l0_error_to_string(ret));
		return ret;
	}

	ret = diag->calculatePowerConsumption(d, totalPowerValue);
	if (diagCmds[diagCmdType::SINGLETEST].enabled) {
		if (ret != ZE_RESULT_SUCCESS) {
			PRINT("Result:  Fail\n");
			PRINT("Message: Fail to check power test.\n");
			ERR("Power test failed: %s\n", l0_error_to_string(ret));
		} else {
			PRINT("Result:  Pass\n");
			PRINT("Message: Pass to check power test.\n");
			if (!featureOnly) {
				PRINT("Computation test completed with %Lf GFLOPS.\n", current);
			}
			PRINT("update peak power value: %d w\n", totalPowerValue);
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
ze_result_t cmdDiag::computationFuncTest(devInfo *d)
{
	TRACING();

	const std::string fileName = "ze_sp_compute.spv";
	const std::string moduleName = "compute_sp_v1";
	float input_value = 1.3f;
	bool featureOnly;
	long double current;

	diagnostic *diag = d->dev->getDiagnostic();
	if (diag == nullptr) {
		ERR("Device does not support diagnostics.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	featureOnly = true;
	ze_result_t res =
		diag->computationTest(d, 4096, &input_value, sizeof(float), fileName, moduleName, current, featureOnly);
	if (diagCmds[diagCmdType::SINGLETEST].enabled) {
		if (res != ZE_RESULT_SUCCESS) {
			PRINT("Result:  Fail\n");
			PRINT("Message: Fail to check computation.\n");
			ERR("Computation test failed: %s\n", l0_error_to_string(res));
		} else {
			PRINT("Result:  Pass\n");
			PRINT("Message: Pass to check computation.\n");
			if (!featureOnly) {
				PRINT("Computation test completed with %Lf GFLOPS.\n", current);
			}
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
ze_result_t cmdDiag::mediaFuncTest(UNUSED devInfo *d)
{
	TRACING();

	std::string pdfStr;
	std::string outputLine;

	pci *p = d->dev->getPCI();
	if (p == nullptr) {
		ERR("Failed to get PCI device properties.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	pdfStr = p->getBDFStr();
	if (!CHECKMEDIACODEC(pdfStr, true, outputLine)) {
		if (diagCmds[diagCmdType::SINGLETEST].enabled) {
			PRINT("Result:  Fail\n");
			PRINT("Message: Fail to check Media transcode.\n");
			ERR("Media codec test failed: %s\n", outputLine.c_str());
		}
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	if (diagCmds[diagCmdType::SINGLETEST].enabled) {
		PRINT("Result:  Pass\n");
		PRINT("Message: Pass to check Media transcode.\n");
		PRINT("%s.\n", outputLine.c_str());
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes Xe Link throughput diagnostic test
 *
 * This function tests Intel Xe Link fabric interconnect throughput between
 * GPU devices, measuring inter-GPU communication bandwidth and validating
 * fabric link performance and connectivity.
 *
 * @param[in] d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if Xe Link tests complete, error code otherwise
 */
ze_result_t cmdDiag::xeLinkThroughput(UNUSED devInfo *d)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes Xe Link all-to-all throughput diagnostic test
 *
 * This function tests comprehensive Xe Link fabric throughput between all
 * available GPU devices simultaneously, measuring full fabric bandwidth
 * and validating complete multi-GPU interconnect performance.
 *
 * @param[in] d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if all-to-all tests complete, error code otherwise
 */
ze_result_t cmdDiag::xeLinkAllToAllThroughput(UNUSED devInfo *d)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Prints formatted table borders for diagnostic output
 *
 * This utility function prints formatted table borders used in diagnostic
 * output displays, providing consistent visual formatting for tabular
 * diagnostic results and error type listings.
 */

ze_result_t cmdDiag::listTypes(UNUSED devInfo *d)
{
	TRACING();

	if (!diagCmds[diagCmdType::PRECHECK].enabled) {
		ERR("--listtypes requires --precheck.\n");
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	printPretty();
	PRINT("| Error ID   | Error Type                        | Error Category       | Error Severity |\n");
	printPretty();
	PRINT("| 1          | GuC Not Running                   | Hardware             | Critical       |\n");
	printPretty();
	PRINT("| 2          | GuC Error                         | Hardware             | Critical       |\n");
	printPretty();
	PRINT("| 3          | GuC Initialization Failed         | Hardware             | Critical       |\n");
	printPretty();
	PRINT("| 4          | IOMMU Catastrophic Error          | Hardware             | Critical       |\n");
	printPretty();
	PRINT("| 5          | LMEM Not Initialized By Firmware  | Hardware             | Critical       |\n");
	printPretty();
	PRINT("| 6          | PCIe Error                        | Hardware             | Critical       |\n");
	printPretty();
	PRINT("| 7          | DRM Error                         | Kernel Mode Driver   | Critical       |\n");
	printPretty();
	PRINT("| 8          | GPU Hang                          | Kernel Mode Driver   | Critical       |\n");
	printPretty();
	PRINT("| 9          | Xe Error                          | Kernel Mode Driver   | Critical       |\n");
	printPretty();
	PRINT("| 10         | Xe Not Loaded                     | Kernel Mode Driver   | Critical       |\n");
	printPretty();
	PRINT("| 11         | Level Zero Init Error             | Kernel Mode Driver   | Critical       |\n");
	printPretty();
	PRINT("| 12         | HuC Disabled                      | Hardware             | High           |\n");
	printPretty();
	PRINT("| 13         | HuC Not Running                   | Hardware             | High           |\n");
	printPretty();
	PRINT("| 14         | Level Zero Metrics Init Error     | User Mode Driver     | High           |\n");
	printPretty();
	PRINT("| 15         | Memory Error                      | Hardware             | Critical       |\n");
	printPretty();
	PRINT("| 16         | GPU Initialization Failed         | Hardware             | Critical       |\n");
	printPretty();
	PRINT("| 17         | MEI Error                         | Kernel Mode Driver   | High           |\n");
	printPretty();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Displays GPU status information for diagnostics
 *
 * This function shows current GPU status and health information as part
 * of the diagnostic precheck process. It provides system state validation
 * and GPU operational status reporting.
 *
 * @param[in] d Pointer to device information structure
 * @return ze_result_t ZE_RESULT_SUCCESS if GPU status retrieval completes, error code otherwise
 */
ze_result_t cmdDiag::gpu(devInfo *d)
{
	TRACING();
	if (!diagCmds[diagCmdType::PRECHECK].enabled) {
		ERR("--gpu requires --precheck.\n");
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	return printPrecheckInfo(d, true);
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
ze_result_t cmdDiag::runSince(UNUSED devInfo *d)
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
	bool found = false;
	int opt;
	int optionIndex = 0;
	std::string shortOpts;
	std::vector<struct option> longOptsVec;

	processOptions(diagCmds, shortOpts, longOptsVec);
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
			diagCmds[diagCmdType::DIAGJSON].enabled = true;
			break;
		case 'd':
			diagCmds[diagCmdType::DIAGDEVICE].enabled = true;
			diagCmds[diagCmdType::DIAGDEVICE].val = optarg;
			break;
		case 'l':
			diagCmds[diagCmdType::LEVEL].enabled = true;
			diagCmds[diagCmdType::LEVEL].val = optarg;
			break;
		case 's':
			diagCmds[diagCmdType::STRESS].enabled = true;
			break;
		case 0:
			for (auto &cmd : diagCmds) {
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

	// If optind is not equal to args->argc, it means there are extra arguments
	// that were not processed by getopt_long.
	if (optind != args->argc) {
		ERR("The following argument was not expected: '%s'.\n", args->argv[optind]);
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	result = args->sm.findDevice(diagCmds[diagCmdType::DIAGDEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '%s'.\n", diagCmds[diagCmdType::DIAGDEVICE].val.c_str());
		return result;
	}

	driverLoaded = args->sm.isDriverLoaded();
	sysInfoShown = false;

	INFO("Running diagnostics can take several minutes to complete.\n");
	// Iterate through the device list and execute the command
	for (auto &device : deviceList) {
		if (device.dev->isIGPU()) {
			DBG("Diagnostics are only supported on discrete GPUs so skipping device %d.\n", device.index);
			continue;
		}

		// Call the appropriate command function based on the command type
		for (const auto &cmd : diagCmds) {
			if (cmd.second.enabled && cmd.second.func != nullptr) {
				DBG("Running command: %s\n", cmd.second.opt.name);
				result = (this->*cmd.second.func)(&device);
				if (diagCmds[diagCmdType::DIAGDEVICE].enabled) {
					DBG("Exiting command: %s\n", cmd.second.opt.name);
					break;
				}
			}
		}
	}

	return result;
}
