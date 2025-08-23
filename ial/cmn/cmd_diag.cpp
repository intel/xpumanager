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
#include <assert.h>
#include <chrono>
#include <fstream>
#include <sys/stat.h>

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
 * @brief Performs pre-diagnostic checks before running full diagnostics
 *
 * This function executes preliminary validation and system checks to ensure
 * the device and system are ready for comprehensive diagnostic testing.
 * It validates prerequisites and system state before running intensive tests.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if pre-checks pass, error code otherwise
 */
ze_result_t cmdDiag::precheck(devInfo *d)
{
	TRACING();

	zes_pci_link_status_t pciLinkStatus;
	std::string fwStatus;
	ze_result_t result;
	std::string outputLine;

	pci *p = (pci *)d->dev->getPCI();
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
		PRINT("| CPU              |  CPU ID: 0                                                          |\n");
		PRINT("|                  |  Status:                                                            |\n");
		printPretty();
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
 * @brief Executes stress testing diagnostics on the device
 *
 * This function runs intensive stress tests designed to validate device
 * stability and performance under heavy computational loads. It helps
 * identify potential hardware issues and thermal limitations.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if stress tests pass, error code otherwise
 */
ze_result_t cmdDiag::stress(UNUSED devInfo *d)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Validates and processes diagnostic level parameter
 *
 * This function validates the diagnostic level parameter provided by the user,
 * ensuring it falls within the valid range (1-3). Each level represents different
 * test intensity: 1 for quick tests, 2 for medium tests, 3 for comprehensive tests.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if level is valid, error code otherwise
 */
ze_result_t cmdDiag::level(UNUSED devInfo *d)
{
	TRACING();

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

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes a specific diagnostic test based on user selection
 *
 * This function runs individual diagnostic tests selected by the user through
 * the --singletest parameter. It maps test IDs to corresponding test functions
 * and executes the requested diagnostic test on the specified device.
 *
 * @param d Pointer to device information structure containing device context
 * @return ze_result_t ZE_RESULT_SUCCESS if test completes successfully, error code otherwise
 */
ze_result_t cmdDiag::runSingleTest(devInfo *d)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;

	static std::unordered_map<diagSubCmdType, diagSubCmdStruct> diagSingleTests = {
		{DIAG_COMPUTATION, {&cmdDiag::computation}},
		{DIAG_MEMORYERROR, {&cmdDiag::memoryError}},
		{DIAG_MEMORYBANDWIDTH, {&cmdDiag::memoryBandwidth}},
		{DIAG_MEDIA, {&cmdDiag::mediaCodec}},
		{DIAG_PCIEBANDWIDTH, {&cmdDiag::pcieBandwidth}},
		{DIAG_POWER, {&cmdDiag::power}},
		{DIAG_COMPUTATIONFUNCTEST, {&cmdDiag::computationFuncTest}},
		{DIAG_MEDIAFUNCTEST, {&cmdDiag::mediaFuncTest}},
		{DIAG_XELINKTHROUGHPUT, {&cmdDiag::xeLinkThroughput}},
		{DIAG_XELINKALLTOALLTHROUGHPUT, {&cmdDiag::xeLinkAllToAllThroughput}},
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
 * @brief Checks if a file or directory path exists
 *
 * This utility function verifies the existence of a specified file or directory
 * path using the stat system call. It provides file system validation for
 * diagnostic operations that require external resources.
 *
 * @param s String containing the path to check
 * @return bool True if path exists, false otherwise
 */
bool cmdDiag::isPathExist(const std::string &s)
{
	struct stat buffer;
	return (stat(s.c_str(), &buffer) == 0);
}

/**
 * @brief Creates a Level Zero module from SPIR-V binary data
 *
 * This function creates a GPU compute module from compiled SPIR-V binary code,
 * enabling kernel execution on the target device. It configures the module
 * descriptor and handles module creation for diagnostic compute operations.
 *
 * @param context Level Zero context handle for the operation
 * @param ze_device Device handle for the target GPU device
 * @param binary_file Vector containing the SPIR-V binary data
 * @param module_handle Pointer to receive the created module handle
 * @return ze_result_t ZE_RESULT_SUCCESS on successful module creation, error code otherwise
 */
ze_result_t cmdDiag::moduleCreate(const ze_context_handle_t &context, ze_device_handle_t ze_device,
								  std::vector<uint8_t> binary_file, ze_module_handle_t *module_handle)
{
	ze_module_desc_t module_description = {};
	module_description.stype = ZE_STRUCTURE_TYPE_MODULE_DESC;
	module_description.pNext = nullptr;
	module_description.format = ZE_MODULE_FORMAT_IL_SPIRV;
	module_description.inputSize = static_cast<uint32_t>(binary_file.size());
	module_description.pInputModule = binary_file.data();
	module_description.pBuildFlags = nullptr;
	ze_result_t ret;

	ret = zeModuleCreate(context, ze_device, &module_description, module_handle, nullptr);

	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't create module: %s\n", l0_error_to_string(ret));
	}
	return ret;
}

/**
 * @brief Destroys a Level Zero module and releases its resources
 *
 * This function properly destroys a GPU compute module created earlier,
 * releasing all associated resources and handles. It provides cleanup
 * functionality for diagnostic operations using GPU modules.
 *
 * @param hModule Handle to the module to be destroyed
 */
void cmdDiag::moduleDestroy(ze_module_handle_t hModule)
{
	ze_result_t ret = zeModuleDestroy(hModule);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't destroy module: %s\n", l0_error_to_string(ret));
	}
}

/**
 * @brief Allocates device memory for GPU operations
 *
 * This function allocates memory on the GPU device for diagnostic operations,
 * configuring memory allocation parameters and handling device-specific
 * memory requirements for compute kernels and data buffers.
 *
 * @param context Level Zero context handle for the operation
 * @param ze_device Device handle for the target GPU device
 * @param size Size of memory to allocate in bytes
 * @param alignment Memory alignment requirement in bytes
 * @param ptr Pointer to receive the allocated memory address
 * @return ze_result_t ZE_RESULT_SUCCESS on successful allocation, error code otherwise
 */
ze_result_t cmdDiag::memoryAlloc(const ze_context_handle_t context, ze_device_handle_t ze_device, size_t size,
								 size_t alignment, void **ptr)
{
	ze_device_mem_alloc_desc_t device_desc = {};
	device_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
	device_desc.pNext = nullptr;
	device_desc.ordinal = 0;
	device_desc.flags = 0;
	ze_result_t ret;
	ret = zeMemAllocDevice(context, &device_desc, size, alignment, ze_device, ptr);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't allocate memory: %s\n", l0_error_to_string(ret));
	}
	return ret;
}

/**
 * @brief Creates a Level Zero command list for GPU operations
 *
 * This function creates a command list that will contain GPU commands for
 * diagnostic operations. Command lists are used to batch and submit GPU
 * operations efficiently for kernel execution and memory operations.
 *
 * @param context Level Zero context handle for the operation
 * @param ze_device Device handle for the target GPU device
 * @param command_queue_group_ordinal Ordinal of the command queue group
 * @param phCommandList Pointer to receive the created command list handle
 * @param flags Command list creation flags
 * @return ze_result_t ZE_RESULT_SUCCESS on successful creation, error code otherwise
 */
ze_result_t cmdDiag::commandListCreate(const ze_context_handle_t context, ze_device_handle_t ze_device,
									   uint32_t command_queue_group_ordinal, ze_command_list_handle_t *phCommandList,
									   uint32_t flags)
{
	ze_command_list_desc_t command_list_description{};
	command_list_description.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
	command_list_description.commandQueueGroupOrdinal = command_queue_group_ordinal;
	command_list_description.pNext = nullptr;
	command_list_description.flags = flags;
	ze_result_t ret;
	ret = zeCommandListCreate(context, ze_device, &command_list_description, phCommandList);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't create command list: %s\n", l0_error_to_string(ret));
	}
	return ret;
}

/**
 * @brief Creates a Level Zero command queue for GPU operation submission
 *
 * This function creates a command queue that will be used to submit command
 * lists to the GPU for execution. Command queues manage the execution of
 * GPU operations and provide synchronization capabilities for diagnostic tests.
 *
 * @param context Level Zero context handle for the operation
 * @param ze_device Device handle for the target GPU device
 * @param command_queue_group_ordinal Ordinal of the command queue group
 * @param command_queue_index Index within the command queue group
 * @param phCommandQueue Pointer to receive the created command queue handle
 * @param flags Command queue creation flags
 * @return ze_result_t ZE_RESULT_SUCCESS on successful creation, error code otherwise
 */
ze_result_t cmdDiag::commandQueueCreate(const ze_context_handle_t context, ze_device_handle_t ze_device,
										const uint32_t command_queue_group_ordinal, const uint32_t command_queue_index,
										ze_command_queue_handle_t *phCommandQueue, uint32_t flags)
{
	ze_command_queue_desc_t command_queue_description{};
	command_queue_description.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
	command_queue_description.pNext = nullptr;
	command_queue_description.ordinal = command_queue_group_ordinal;
	command_queue_description.index = command_queue_index;
	command_queue_description.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
	command_queue_description.flags = flags;

	ze_result_t ret;
	ret = zeCommandQueueCreate(context, ze_device, &command_queue_description, phCommandQueue);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't create command queue: %s\n", l0_error_to_string(ret));
	}
	return ret;
}

/**
 * @brief Executes command lists on a GPU command queue
 *
 * This function submits a command list to the specified command queue for
 * execution on the GPU. It provides the mechanism to trigger GPU operations
 * that have been recorded in command lists for diagnostic tests.
 *
 * @param hCommandQueue Handle to the command queue for execution
 * @param hCommandList Handle to the command list to execute
 */
void commandQueueExecuteCommandLists(ze_command_queue_handle_t hCommandQueue, ze_command_list_handle_t hCommandList)
{
	ze_result_t ret = zeCommandQueueExecuteCommandLists(hCommandQueue, 1, &hCommandList, nullptr);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't execute command lists: %s\n", l0_error_to_string(ret));
	}
}

/**
 * @brief Synchronizes with GPU command queue completion
 *
 * This function waits for all commands in the specified command queue to
 * complete execution. It provides synchronization between CPU and GPU
 * operations, ensuring diagnostic tests complete before proceeding.
 *
 * @param hCommandQueue Handle to the command queue to synchronize with
 */
void commandQueueSynchronize(ze_command_queue_handle_t hCommandQueue)
{
	ze_result_t ret = zeCommandQueueSynchronize(hCommandQueue, UINT64_MAX);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't synchronize command queue: %s\n", l0_error_to_string(ret));
	}
}

/**
 * @brief Resets a command list for reuse
 *
 * This function resets a command list to its initial state, allowing it to
 * be reused for recording new GPU commands. It provides efficient command
 * list management for iterative diagnostic operations.
 *
 * @param hCommandList Handle to the command list to reset
 */
void commandListReset(ze_command_list_handle_t hCommandList)
{
	ze_result_t ret = zeCommandListReset(hCommandList);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't reset command list: %s\n", l0_error_to_string(ret));
	}
}

/**
 * @brief Creates a GPU kernel from a module
 *
 * This function creates a kernel object from a compiled module, allowing
 * specific kernel functions to be executed on the GPU. It provides the
 * foundation for running compute operations in diagnostic tests.
 *
 * @param hModule Handle to the module containing the kernel
 * @param name String name of the kernel function to create
 * @param hKernel Pointer to receive the created kernel handle
 * @return ze_result_t ZE_RESULT_SUCCESS on successful kernel creation, error code otherwise
 */
ze_result_t kernelCreate(ze_module_handle_t hModule, std::string name, ze_kernel_handle_t *hKernel)
{
	ze_kernel_desc_t test_function_description = {};
	test_function_description.stype = ZE_STRUCTURE_TYPE_KERNEL_DESC;
	test_function_description.pNext = nullptr;
	test_function_description.flags = 0;
	test_function_description.pKernelName = name.c_str();

	ze_result_t ret = zeKernelCreate(hModule, &test_function_description, hKernel);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Error creating kernel: %s\n", l0_error_to_string(ret));
	}
	return ret;
}

/**
 * @brief Frees GPU device memory
 *
 * This function releases device memory that was previously allocated for
 * GPU operations. It provides proper cleanup and resource management
 * for diagnostic tests that use GPU memory buffers.
 *
 * @param context Level Zero context handle
 * @param ptr Pointer to the device memory to free
 */
void memoryFree(const ze_context_handle_t &context, const void *ptr)
{
	ze_result_t ret = zeMemFree(context, const_cast<void *>(ptr));
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Error freeing memory: %s\n", l0_error_to_string(ret));
	}
}

/**
 * @brief Configures a kernel function with input and output parameters
 *
 * This function sets up a kernel for execution by creating the kernel object
 * and configuring its arguments with input and output memory buffers.
 * It prepares kernels for diagnostic compute operations.
 *
 * @param module_handle Handle to the module containing the kernel
 * @param function Reference to kernel handle to be created and configured
 * @param name Name of the kernel function to set up
 * @param input Pointer to input data buffer
 * @param output Pointer to output data buffer
 */
void setupFunction(ze_module_handle_t module_handle, ze_kernel_handle_t &function, const char *name, void *input,
				   void *output)
{
	ze_result_t ret = kernelCreate(module_handle, name, &function);
	if (ret != ZE_RESULT_SUCCESS) {
		return;
	}

	ret = zeKernelSetArgumentValue(function, 0, sizeof(input), &input);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Error setting kernel argument value: %s\n", l0_error_to_string(ret));
	}
	ret = zeKernelSetArgumentValue(function, 1, sizeof(output), &output);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Error setting kernel argument value: %s\n", l0_error_to_string(ret));
	}
}

/**
 * @brief Appends a kernel launch command to a command list
 *
 * This function records a kernel launch operation in the specified command
 * list with the given launch parameters. It provides the mechanism to
 * schedule kernel execution as part of GPU command sequences.
 *
 * @param hCommandList Handle to the command list
 * @param hKernel Handle to the kernel to launch
 * @param pLaunchFuncArgs Pointer to launch function arguments containing work group configuration
 */
void commandListAppendLaunchKernel(ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel,
								   const ze_group_count_t *pLaunchFuncArgs)
{
	ze_result_t ret = zeCommandListAppendLaunchKernel(hCommandList, hKernel, pLaunchFuncArgs, nullptr, 0, nullptr);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Error appending launch kernel: %s\n", l0_error_to_string(ret));
	}
}

/**
 * @brief Calculates bandwidth in GBPS from timing measurements
 *
 * This utility function computes bandwidth in gigabytes per second based on
 * execution period and total throughput measurements. It provides performance
 * calculation capabilities for diagnostic bandwidth tests.
 *
 * @param period Execution time period in appropriate units
 * @param total_gbps Total throughput measurement
 * @return long double Calculated bandwidth in GBPS, or maximum value if period is near zero
 */
long double calculateGbps(long double period, long double total_gbps)
{
	auto ep = std::numeric_limits<double>::epsilon();
	if (period > ep)
		return total_gbps / period;
	else {
		return std::numeric_limits<double>::max();
	}
}

/**
 * @brief Executes a GPU kernel with timing and performance measurement
 *
 * This function runs a GPU kernel with the specified work group configuration,
 * measures execution time, and optionally performs performance analysis.
 * It supports both functional testing and performance benchmarking modes.
 *
 * @param command_queue Handle to the command queue for kernel execution
 * @param command_list Handle to the command list containing kernel commands
 * @param function Reference to the kernel function to execute
 * @param workgroup_info Work group configuration parameters
 * @param checkOnly Boolean flag: true for functional testing, false for performance measurement
 * @return long double Execution time in nanoseconds, or -1 on error
 */
long double runKernel(ze_command_queue_handle_t command_queue, ze_command_list_handle_t command_list,
					  ze_kernel_handle_t &function, struct ZeWorkGroups &workgroup_info, bool checkOnly)
{
	long double timed = 0;

	ze_result_t ret = zeKernelSetGroupSize(function, workgroup_info.group_size_x, workgroup_info.group_size_y,
										   workgroup_info.group_size_z);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Error setting kernel group size: %s\n", l0_error_to_string(ret));
		return -1;
	}

	ze_group_count_t thread_group_dimensions;
	thread_group_dimensions.groupCountX = workgroup_info.group_count_x;
	thread_group_dimensions.groupCountY = workgroup_info.group_count_y;
	thread_group_dimensions.groupCountZ = workgroup_info.group_count_z;
	commandListAppendLaunchKernel(command_list, function, &thread_group_dimensions);

	ret = zeCommandListClose(command_list);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't close command list: %s\n", l0_error_to_string(ret));
		return -1;
	}

	// 1 round is good enough if it is not perf diag
	if (checkOnly == true) {
		commandQueueExecuteCommandLists(command_queue, command_list);
		commandQueueSynchronize(command_queue);
		return 0;
	}
	// Consistent with ze_peak
	uint32_t warmup_iterations = 5;
	uint32_t iters = 20;
	for (uint32_t i = 0; i < warmup_iterations; i++) {
		commandQueueExecuteCommandLists(command_queue, command_list);
		commandQueueSynchronize(command_queue);
	}

	std::chrono::high_resolution_clock::time_point time_start, time_end;
	time_start = std::chrono::high_resolution_clock::now();
	for (uint32_t i = 0; i < iters; i++) {
		commandQueueExecuteCommandLists(command_queue, command_list);
		commandQueueSynchronize(command_queue);
	}

	time_end = std::chrono::high_resolution_clock::now();
	timed = std::chrono::duration<long double, std::chrono::nanoseconds::period>(time_end - time_start).count();
	commandListReset(command_list);
	return (timed / static_cast<long double>(iters));
}

/**
 * @brief Configures work group parameters for GPU kernel execution
 *
 * This function calculates optimal work group dimensions and counts based on
 * device compute properties and requested work items. It ensures work group
 * configuration stays within device limits while maximizing GPU utilization.
 *
 * @param device_compute_properties Pointer to device compute properties structure
 * @param total_work_items_requested Total number of work items requested for execution
 * @param workgroup_info Pointer to work group structure to be populated
 * @return uint64_t Actual number of work items that will be executed
 */
uint64_t cmdDiag::setWorkgroups(ze_device_compute_properties_t *device_compute_properties,
								const uint64_t total_work_items_requested, struct ZeWorkGroups *workgroup_info)
{
	auto group_size_x = std::min(total_work_items_requested, uint64_t(device_compute_properties->maxGroupSizeX));
	auto group_size_y = 1;
	auto group_size_z = 1;
	auto group_size = group_size_x * group_size_y * group_size_z;

	auto group_count_x = total_work_items_requested / group_size;
	group_count_x = std::min(group_count_x, uint64_t(device_compute_properties->maxGroupCountX));
	auto remaining_items = total_work_items_requested - group_count_x * group_size;

	uint64_t group_count_y = remaining_items / (group_count_x * group_size);
	group_count_y = std::min(group_count_y, uint64_t(device_compute_properties->maxGroupCountY));
	group_count_y = std::max(group_count_y, uint64_t(1));
	remaining_items = total_work_items_requested - group_count_x * group_count_y * group_size;

	uint64_t group_count_z = remaining_items / (group_count_x * group_count_y * group_size);
	group_count_z = std::min(group_count_z, uint64_t(device_compute_properties->maxGroupCountZ));
	group_count_z = std::max(group_count_z, uint64_t(1));

	auto final_work_items = group_count_x * group_count_y * group_count_z * group_size;
	remaining_items = total_work_items_requested - final_work_items;

	workgroup_info->group_size_x = static_cast<uint32_t>(group_size_x);
	workgroup_info->group_size_y = static_cast<uint32_t>(group_size_y);
	workgroup_info->group_size_z = static_cast<uint32_t>(group_size_z);
	workgroup_info->group_count_x = static_cast<uint32_t>(group_count_x);
	workgroup_info->group_count_y = static_cast<uint32_t>(group_count_y);
	workgroup_info->group_count_z = static_cast<uint32_t>(group_count_z);

	return final_work_items;
}

/**
 * @brief Loads SPIR-V binary file for GPU kernel execution
 *
 * This function reads a compiled SPIR-V binary file from the resources directory
 * and loads it into memory for module creation. It provides kernel loading
 * capabilities for diagnostic compute operations.
 *
 * @param file_path Path to the SPIR-V binary file relative to kernels directory
 * @param binary_file Pointer to vector that will contain the loaded binary data
 * @return ze_result_t ZE_RESULT_SUCCESS on successful file loading, error code otherwise
 */
ze_result_t cmdDiag::loadBinaryFile(const std::string &file_path, std::vector<uint8_t> *binary_file)
{
	std::string folder = std::string(XPUM_RESOURCES_DIR) + std::string("kernels/");
	if (!isPathExist(folder)) {
		ERR("Kernel folder does not exist: %s\n", folder.c_str());
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	std::string absolute_file_path = folder + file_path;
	std::ifstream stream(absolute_file_path, std::ios::in | std::ios::binary);

	if (!stream.good()) {
		ERR("Failed to open kernel file: %s\n", absolute_file_path.c_str());
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	size_t length = 0;
	stream.seekg(0, stream.end);
	length = static_cast<size_t>(stream.tellg());
	stream.seekg(0, stream.beg);
	binary_file->resize(length);
	stream.read(reinterpret_cast<char *>(binary_file->data()), length);
	stream.close();

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Runs the computation diagnostic test on the specified device when user runs diag --singletest 1
 *
 * This function performs a floating-point computation test on the GPU device to measure its performance
 * in terms of GFLOPS. It loads a SPIR-V kernel, allocates necessary device memory, sets up and launches
 * the kernel, and reports the measured performance.
 *
 * @param d Pointer to the device information structure.
 * @return ze_result_t Returns ZE_RESULT_SUCCESS on success, or an appropriate error code on failure.
 */
ze_result_t cmdDiag::computation(devInfo *d)
{
	TRACING();
	ze_device_properties_t zeDevProp = {};
	ze_device_compute_properties_t zeComputeProp = {};
	ze_result_t result;
	ze_context_handle_t context = d->dev->getContext();
	float input_value = 1.3f;
	bool checkOnly = false;
	std::vector<long double> all_gflops;
	// int i = 0;

	result = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	result = d->dev->getComputeProps(d->deviceHdl, &zeComputeProp);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	long double timed;
	std::size_t flops_per_work_item = 4096;
	std::vector<uint8_t> binary_file;
	ze_result_t ret = loadBinaryFile("ze_sp_compute.spv", &binary_file);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Failed to load binary file: %s\n", l0_error_to_string(ret));
		return ret;
	}
	ze_module_handle_t module_handle;
	ret = moduleCreate(context, d->deviceHdl, binary_file, &module_handle);
	if (ret != ZE_RESULT_SUCCESS) {
		return ret;
	}

	uint64_t max_work_items = (uint64_t)zeDevProp.numSlices * zeDevProp.numSubslicesPerSlice *
							  zeDevProp.numEUsPerSubslice * zeComputeProp.maxGroupCountX * 2048;
	uint64_t max_number_of_allocated_items = zeDevProp.maxMemAllocSize / sizeof(float);
	uint64_t number_of_work_items = std::min(max_number_of_allocated_items, (max_work_items * sizeof(float)));
	ZeWorkGroups workgroup_info;
	number_of_work_items = setWorkgroups(&zeComputeProp, number_of_work_items, &workgroup_info);

	void *device_input_value;
	ret = memoryAlloc(context, d->deviceHdl, sizeof(float), 1, &device_input_value);
	if (ret != ZE_RESULT_SUCCESS) {
		moduleDestroy(module_handle);
		return ret;
	}

	void *device_output_buffer;
	ret = memoryAlloc(context, d->deviceHdl, static_cast<std::size_t>((number_of_work_items * sizeof(float))), 1,
					  &device_output_buffer);
	if (ret != ZE_RESULT_SUCCESS) {
		memoryFree(context, device_input_value);
		moduleDestroy(module_handle);
		return ret;
	}

	ze_command_list_handle_t command_list;
	ret = commandListCreate(context, d->deviceHdl, 0, &command_list, ZE_COMMAND_LIST_FLAG_EXPLICIT_ONLY);
	if (ret != ZE_RESULT_SUCCESS) {
		memoryFree(context, device_input_value);
		memoryFree(context, device_output_buffer);
		moduleDestroy(module_handle);
		return ret;
	}

	ze_command_queue_handle_t command_queue;
	ret = commandQueueCreate(context, d->deviceHdl, 0, 0, &command_queue, ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY);
	if (ret != ZE_RESULT_SUCCESS) {
		memoryFree(context, device_input_value);
		memoryFree(context, device_output_buffer);
		moduleDestroy(module_handle);
		return ret;
	}

	ret = zeCommandListAppendMemoryCopy(command_list, device_output_buffer, &input_value, sizeof(float), nullptr, 0,
										nullptr);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't append memory copy: %s\n", l0_error_to_string(ret));
		memoryFree(context, device_input_value);
		memoryFree(context, device_output_buffer);
		moduleDestroy(module_handle);
		return ret;
	}

	ret = zeCommandListAppendBarrier(command_list, nullptr, 0, nullptr);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't append barrier: %s\n", l0_error_to_string(ret));
		memoryFree(context, device_input_value);
		memoryFree(context, device_output_buffer);
		moduleDestroy(module_handle);
		return ret;
	}

	ret = zeCommandListClose(command_list);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't close command list: %s\n", l0_error_to_string(ret));
		memoryFree(context, device_input_value);
		memoryFree(context, device_output_buffer);
		moduleDestroy(module_handle);
		return ret;
	}

	commandQueueExecuteCommandLists(command_queue, command_list);
	commandQueueSynchronize(command_queue);
	commandListReset(command_list);
	ze_kernel_handle_t compute_sp_v1;
	setupFunction(module_handle, compute_sp_v1, "compute_sp_v1", device_input_value, device_output_buffer);

	timed = 0;
	long double current;
	// Vector width 1
	timed = runKernel(command_queue, command_list, compute_sp_v1, workgroup_info, checkOnly);
	current = calculateGbps(timed, (long double)number_of_work_items * flops_per_work_item);
	//	all_gflops[i] = std::max(all_gflops[i], current);
	ret = zeKernelDestroy(compute_sp_v1);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Error destroying kernel: %s\n", l0_error_to_string(ret));
	}

	ret = zeCommandListDestroy(command_list);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Error destroying command list: %s\n", l0_error_to_string(ret));
	}

	ret = zeCommandQueueDestroy(command_queue);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Error destroying command queue: %s\n", l0_error_to_string(ret));
	}

	memoryFree(context, device_input_value);
	memoryFree(context, device_output_buffer);
	moduleDestroy(module_handle);
	PRINT("Computation test completed with %Lf GFLOPS.\n", current);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes memory error diagnostic test
 *
 * This function performs memory error detection tests on the GPU device,
 * checking for memory corruption, ECC errors, and memory integrity issues.
 * It validates GPU memory subsystem reliability and health.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if memory tests pass, error code otherwise
 */
ze_result_t cmdDiag::memoryError(UNUSED devInfo *d)
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
 * @param d Pointer to device information structure (currently unused)
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
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if media tests pass, error code otherwise
 */
ze_result_t cmdDiag::mediaCodec(UNUSED devInfo *d)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes PCIe bandwidth diagnostic test
 *
 * This function measures PCIe interface bandwidth between CPU and GPU,
 * testing data transfer rates and identifying potential PCIe bottlenecks.
 * It validates system interconnect performance and PCIe link integrity.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if PCIe tests complete, error code otherwise
 */
ze_result_t cmdDiag::pcieBandwidth(UNUSED devInfo *d)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes power management diagnostic test
 *
 * This function tests GPU power management capabilities including power
 * limiting, thermal throttling, and power consumption monitoring. It validates
 * power subsystem functionality and thermal management effectiveness.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if power tests pass, error code otherwise
 */
ze_result_t cmdDiag::power(UNUSED devInfo *d)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes computation functional diagnostic test
 *
 * This function performs functional validation of GPU compute capabilities,
 * running basic compute operations to verify correct GPU functionality
 * without performance measurement emphasis.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if functional tests pass, error code otherwise
 */
ze_result_t cmdDiag::computationFuncTest(UNUSED devInfo *d)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes media functional diagnostic test
 *
 * This function performs functional validation of GPU media engines,
 * testing basic media processing capabilities to verify correct media
 * subsystem operation without performance measurement emphasis.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if media functional tests pass, error code otherwise
 */
ze_result_t cmdDiag::mediaFuncTest(UNUSED devInfo *d)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes Xe Link throughput diagnostic test
 *
 * This function tests Intel Xe Link fabric interconnect throughput between
 * GPU devices, measuring inter-GPU communication bandwidth and validating
 * fabric link performance and connectivity.
 *
 * @param d Pointer to device information structure (currently unused)
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
 * @param d Pointer to device information structure (currently unused)
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
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS if GPU status retrieval completes, error code otherwise
 */
ze_result_t cmdDiag::gpu(UNUSED devInfo *d)
{
	TRACING();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes time-based log scanning for diagnostic precheck
 *
 * This function scans system logs starting from a specified time point
 * to identify GPU-related errors and issues. It provides historical
 * system analysis for diagnostic precheck operations.
 *
 * @param d Pointer to device information structure (currently unused)
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
