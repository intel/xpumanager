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

#include <device.h>
#include <vector>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <thread>
#include <cmath>
#include <chrono>
#include "diagnostic.h"
#include "file_io.h"

/**
 * @brief Destructor for the diagnostic class
 *
 * This destructor performs cleanup operations for the diagnostic management
 * object, releasing allocated memory for diagnostic test suite handles and
 * ensuring proper resource deallocation when the diagnostic object is destroyed.
 */
diagnostic::~diagnostic()
{
	if (testSuites) {
		delete[] testSuites;
		testSuites = nullptr;
	}
}

/**
 * @brief Enumerates available diagnostic test suites for a device
 *
 * This function discovers and catalogs all diagnostic test suites available on
 * the specified device. Diagnostic test suites provide comprehensive hardware
 * validation and health checking capabilities for GPU components.
 *
 * @param[in] device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
ze_result_t diagnostic::enumDiag(zes_device_handle_t device)
{
	// Enumerate diagnostic test suites
	ze_result_t result = zesDeviceEnumDiagnosticTestSuites(device, &testSuiteCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || testSuiteCount == 0) {
		ERR("Failed to enumerate diagnostic test suites or no test suites found: 0x%X (%s)\n", result,
			l0_error_to_string(result));
		return result;
	}

	testSuites = new zes_diag_handle_t[testSuiteCount];
	result = zesDeviceEnumDiagnosticTestSuites(device, &testSuiteCount, testSuites);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get diagnostic test suite handles: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("  - Device has %d diagnostic test suites:\n", testSuiteCount);
	return result;
}

/**
 * @brief Gets properties for a specific diagnostic test suite
 *
 * This function retrieves detailed properties and characteristics for a
 * specific diagnostic test suite, including suite name, type, test availability,
 * and subdevice association information for comprehensive diagnostic analysis.
 *
 * @param[in] testSuite Handle to the specific diagnostic test suite
 * @return ze_result_t ZE_RESULT_SUCCESS on successful property retrieval, error code otherwise
 */
ze_result_t diagnostic::getProperties(zes_diag_handle_t testSuite)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_diag_properties_t diagProperties = {};
	result = zesDiagnosticsGetProperties(testSuite, &diagProperties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get diagnostic test suite properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	DBG("    - Test Suite Name: %s\n", diagProperties.name);
	DBG("    - Type : %d\n", diagProperties.stype);
	DBG("    - have Tests : %d\n", diagProperties.haveTests);
	DBG("    - onSubDevice : %d\n", diagProperties.onSubdevice);
	DBG("    - subdeviceId : %d\n", diagProperties.subdeviceId);
	return result;
}

/**
 * @brief Calculates bandwidth in GBPS from timing measurements
 *
 * This utility function computes bandwidth in gigabytes per second based on
 * execution period and total throughput measurements. It provides performance
 * calculation capabilities for diagnostic bandwidth tests.
 *
 * @param[in] period Execution time period in appropriate units
 * @param[in] total_gbps Total throughput measurement
 * @return long double Calculated bandwidth in GBPS, or maximum value if period is near zero
 */
long double calculateGbps(long double period, long double totalGbps)
{
	constexpr auto ep = std::numeric_limits<long double>::epsilon();
	if ((period * 10e3) > (ep * 10e3))
		return totalGbps / period;
	else {
		return std::numeric_limits<long double>::max();
	}
}

/**
 * @brief Calculates bandwidth and latency from timing measurements.
 *
 * This utility function computes bandwidth (in gigabytes per second) and latency (in microseconds)
 * based on execution time and total data transfer measurements. It is used for diagnostic
 * bandwidth and latency tests.
 *
 * @param[in]  totalTimeNsec      Total execution time in nanoseconds.
 * @param[in]  totalDataTransfer  Total data transferred in bytes.
 * @param[out] totalBandwidth     Calculated bandwidth in gigabytes per second.
 * @param[out] totalLatency       Calculated latency in microseconds.
 * @param[in]  numberIterations   Number of iterations executed.
 * @return Calculated bandwidth and latency
 */
void calculateBandwidthLatency(long double totalTimeNsec, long double totalDataTransfer, long double &totalBandwidth,
							   long double &totalLatency, std::size_t numberIterations)
{
	long double totalTimeS;
	totalTimeS = totalTimeNsec / 1e9; /* Units in Seconds */
	totalDataTransfer /= 1e9L;		  /* Units in Gigabytes */

	totalBandwidth = totalDataTransfer / totalTimeS;
	totalLatency = totalTimeNsec / (1e3 * static_cast<double>(numberIterations));
}

/**
 * @brief Performs a computation test on a device to evaluate its floating-point performance.
 *
 * This function executes a single-precision floating-point computation test on the specified device
 * using Level Zero APIs. It loads and executes a shader program ("ze_sp_compute.spv") that performs
 * intensive floating-point calculations, then measures the performance in GFLOPS.
 *
 * The test:
 * 1. Retrieves device properties
 * 2. Loads the shader binary file
 * 3. Creates necessary memory allocations
 * 4. Sets up command lists and queues
 * 5. Configures workgroups based on device capabilities
 * 6. Executes the kernel and measures performance
 * 7. Cleans up resources
 *
 * @param[in] d Pointer to the device information structure containing the device handle and context
 * @param[in] flops_per_work_item std::size_t value for flops per work item
 * @param[in] srcPtr pointer to a specific data type
 * @param[in] srcSize source size for a specific data type
 * @param[in] fileName, reference  to a specific data type based SPV file
 * @param[in] moduleName, reference to a related kernel module name
 * @param[out] out_gflops, reference to a related output long double flops value
 * @param[in] functionalCheck, Boolean flag: true for functional testing, false for performance measurement
 * @return ze_result_t ZE_RESULT_SUCCESS on successful test completion, or an error code on failure
 */
ze_result_t diagnostic::computationTest(devInfo *d, std::size_t flopsPerWorkItem, void *srcPtr, size_t srcSize,
										const std::string &fileName, const std::string &moduleName,
										long double &outGflops, bool functionalCheck)
{
	TRACING();
	ze_device_properties_t zeDevProp = {};
	ze_device_compute_properties_t zeComputeProp = {};
	ze_result_t result;
	ze_context_handle_t context = d->dev->getContext();
	std::vector<long double> allGflops;

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
	std::vector<uint8_t> binaryFile;
	ze_result_t ret = loadBinaryFile(fileName, &binaryFile);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Failed to load binary file: %s\n", l0_error_to_string(ret));
		return ret;
	}
	ze_module_handle_t moduleHandle;
	ret = moduleCreate(context, d->deviceHdl, binaryFile, &moduleHandle);
	if (ret != ZE_RESULT_SUCCESS) {
		return ret;
	}

	uint64_t maxWorkItems = (uint64_t)zeDevProp.numSlices * zeDevProp.numSubslicesPerSlice *
							zeDevProp.numEUsPerSubslice * zeComputeProp.maxGroupCountX * 2048;
	uint64_t maxNumberOfAllocatedItems = zeDevProp.maxMemAllocSize / srcSize;
	uint64_t numberOfWorkItems = std::min(maxNumberOfAllocatedItems, (maxWorkItems * srcSize));
	ZeWorkGroups workgroupInfo;
	numberOfWorkItems = setWorkgroups(&zeComputeProp, numberOfWorkItems, &workgroupInfo);

	void *deviceInputValue;
	ret = memoryAlloc(context, d->deviceHdl, srcSize, 1, &deviceInputValue);
	if (ret != ZE_RESULT_SUCCESS) {
		moduleDestroy(moduleHandle);
		return ret;
	}

	void *deviceOutputBuffer;
	ret = memoryAlloc(context, d->deviceHdl, static_cast<std::size_t>(numberOfWorkItems * srcSize), 1,
					  &deviceOutputBuffer);
	if (ret != ZE_RESULT_SUCCESS) {
		memoryFree(context, deviceInputValue);
		moduleDestroy(moduleHandle);
		return ret;
	}

	ze_command_list_handle_t commandList;
	ret = commandListCreate(context, d->deviceHdl, 0, &commandList, ZE_COMMAND_LIST_FLAG_EXPLICIT_ONLY);
	if (ret != ZE_RESULT_SUCCESS) {
		memoryFree(context, deviceInputValue);
		memoryFree(context, deviceOutputBuffer);
		moduleDestroy(moduleHandle);
		return ret;
	}

	ze_command_queue_handle_t commandQueue;
	ret = commandQueueCreate(context, d->deviceHdl, 0, 0, &commandQueue, ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY);
	if (ret != ZE_RESULT_SUCCESS) {
		memoryFree(context, deviceInputValue);
		memoryFree(context, deviceOutputBuffer);
		moduleDestroy(moduleHandle);
		return ret;
	}

	ret = zeCommandListAppendMemoryCopy(commandList, deviceOutputBuffer, srcPtr, srcSize, nullptr, 0, nullptr);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't append memory copy: %s\n", l0_error_to_string(ret));
		memoryFree(context, deviceInputValue);
		memoryFree(context, deviceOutputBuffer);
		moduleDestroy(moduleHandle);
		return ret;
	}

	ret = zeCommandListAppendBarrier(commandList, nullptr, 0, nullptr);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't append barrier: %s\n", l0_error_to_string(ret));
		memoryFree(context, deviceInputValue);
		memoryFree(context, deviceOutputBuffer);
		moduleDestroy(moduleHandle);
		return ret;
	}

	ret = zeCommandListClose(commandList);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't close command list: %s\n", l0_error_to_string(ret));
		memoryFree(context, deviceInputValue);
		memoryFree(context, deviceOutputBuffer);
		moduleDestroy(moduleHandle);
		return ret;
	}

	commandQueueExecuteCommandLists(commandQueue, commandList);
	commandQueueSynchronize(commandQueue);

	commandListReset(commandList);
	ze_kernel_handle_t computeSpV1;
	setupFunction(moduleHandle, computeSpV1, moduleName, deviceInputValue, deviceOutputBuffer);

	timed = 0;
	// Vector width 1
	timed = runKernel(commandQueue, commandList, computeSpV1, workgroupInfo, functionalCheck);
	if (!functionalCheck) {
		outGflops = calculateGbps(timed, (long double)numberOfWorkItems * flopsPerWorkItem);
	}
	//	all_gflops[i] = std::max(all_gflops[i], current);
	ret = zeKernelDestroy(computeSpV1);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Error destroying kernel: %s\n", l0_error_to_string(ret));
	}

	ret = zeCommandListDestroy(commandList);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Error destroying command list: %s\n", l0_error_to_string(ret));
	}

	ret = zeCommandQueueDestroy(commandQueue);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Error destroying command queue: %s\n", l0_error_to_string(ret));
	}

	memoryFree(context, deviceInputValue);
	memoryFree(context, deviceOutputBuffer);
	moduleDestroy(moduleHandle);

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Creates a Level Zero module from SPIR-V binary data
 *
 * This function creates a GPU compute module from compiled SPIR-V binary code,
 * enabling kernel execution on the target device. It configures the module
 * descriptor and handles module creation for diagnostic compute operations.
 *
 * @param[in] context Level Zero context handle for the operation
 * @param[in] ze_device Device handle for the target GPU device
 * @param[in] binary_file Vector containing the SPIR-V binary data
 * @param[out] module_handle Pointer to receive the created module handle
 * @return ze_result_t ZE_RESULT_SUCCESS on successful module creation, error code otherwise
 */
ze_result_t diagnostic::moduleCreate(const ze_context_handle_t &contextHandle, ze_device_handle_t zeDevice,
									 std::vector<uint8_t> binaryFile, ze_module_handle_t *moduleHandle)
{
	ze_module_desc_t moduleDescription = {};
	moduleDescription.stype = ZE_STRUCTURE_TYPE_MODULE_DESC;
	moduleDescription.pNext = nullptr;
	moduleDescription.format = ZE_MODULE_FORMAT_IL_SPIRV;
	moduleDescription.inputSize = static_cast<uint32_t>(binaryFile.size());
	moduleDescription.pInputModule = binaryFile.data();
	moduleDescription.pBuildFlags = nullptr;
	ze_result_t ret;

	ret = zeModuleCreate(contextHandle, zeDevice, &moduleDescription, moduleHandle, nullptr);

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
 * @param[in] hModule Handle to the module to be destroyed
 */
void diagnostic::moduleDestroy(ze_module_handle_t hModule)
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
 * @param[in] context Level Zero context handle for the operation
 * @param[in] ze_device Device handle for the target GPU device
 * @param[in] size Size of memory to allocate in bytes
 * @param[in] alignment Memory alignment requirement in bytes
 * @param[out] ptr Pointer to receive the allocated memory address
 * @return ze_result_t ZE_RESULT_SUCCESS on successful allocation, error code otherwise
 */
ze_result_t diagnostic::memoryAlloc(const ze_context_handle_t contextHandle, ze_device_handle_t zeDevice, size_t size,
									size_t alignment, void **ptr)
{
	ze_device_mem_alloc_desc_t deviceDesc = {};
	deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
	deviceDesc.pNext = nullptr;
	deviceDesc.ordinal = 0;
	deviceDesc.flags = 0;
	ze_result_t ret;
	ret = zeMemAllocDevice(contextHandle, &deviceDesc, size, alignment, zeDevice, ptr);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't allocate memory: %s\n", l0_error_to_string(ret));
	}
	return ret;
}

/**
 * @brief Allocates host memory for GPU operations
 *
 * This function allocates memory on the GPU device for diagnostic operations,
 * configuring memory allocation parameters and handling host specific
 * memory requirements for compute kernels and data buffers.
 *
 * @param[in] context Level Zero context handle for the operation
 * @param[in] size Size of memory to allocate in bytes
 * @param[in] alignment Memory alignment requirement in bytes
 * @param[out] ptr Pointer to receive the allocated memory address
 * @return ze_result_t ZE_RESULT_SUCCESS on successful allocation, error code otherwise
 */
ze_result_t diagnostic::memoryAllocHost(const ze_context_handle_t contextHandle, size_t size, size_t alignment,
										void **ptr)
{
	ze_host_mem_alloc_desc_t hostDesc = {};
	hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
	hostDesc.pNext = nullptr;
	hostDesc.flags = 0;
	ze_result_t ret;

	ret = zeMemAllocHost(contextHandle, &hostDesc, size, alignment, ptr);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't allocate memory: %s\n", l0_error_to_string(ret));
	}
	return ret;
}

/**
 * @brief Allocates shared memory for GPU operations
 *
 * This function allocates shared memory on the GPU device for diagnostic operations,
 * configuring memory allocation parameters and handling both device and host specific
 * shared memory requirements for compute kernels and data buffers.
 *
 * @param[in] context Level Zero context handle for the operation
 * @param[in] zeDevice Device handle for the target GPU device
 * @param[in] size Size of memory to allocate in bytes
 * @param[in] alignment Memory alignment requirement in bytes
 * @param[out] ptr Pointer to receive the allocated memory address
 * @return ze_result_t ZE_RESULT_SUCCESS on successful allocation, error code otherwise
 */
ze_result_t diagnostic::memoryAllocShared(const ze_context_handle_t &context, ze_device_handle_t zeDevice, size_t size,
										  size_t alignment, void **ptr)
{
	ze_device_mem_alloc_desc_t deviceDescOutput = {};
	deviceDescOutput.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
	deviceDescOutput.ordinal = 0;
	deviceDescOutput.flags = 0;
	deviceDescOutput.pNext = nullptr;

	ze_host_mem_alloc_desc_t hostDescOutput = {};
	hostDescOutput.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
	hostDescOutput.flags = 0;
	hostDescOutput.pNext = nullptr;
	ze_result_t ret;

	ret = zeMemAllocShared(context, &deviceDescOutput, &hostDescOutput, size, alignment, zeDevice, ptr);
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
 * @param[in] context Level Zero context handle for the operation
 * @param[in] ze_device Device handle for the target GPU device
 * @param[in] command_queue_group_ordinal Ordinal of the command queue group
 * @param[out] phCommandList Pointer to receive the created command list handle
 * @param[in] flags Command list creation flags
 * @return ze_result_t ZE_RESULT_SUCCESS on successful creation, error code otherwise
 */
ze_result_t diagnostic::commandListCreate(const ze_context_handle_t contextHandle, ze_device_handle_t zeDevice,
										  uint32_t commandQueueGroupOrdinal, ze_command_list_handle_t *phCommandList,
										  uint32_t flags)
{
	ze_command_list_desc_t commandListDescription{};
	commandListDescription.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
	commandListDescription.commandQueueGroupOrdinal = commandQueueGroupOrdinal;
	commandListDescription.pNext = nullptr;
	commandListDescription.flags = flags;
	ze_result_t ret;
	ret = zeCommandListCreate(contextHandle, zeDevice, &commandListDescription, phCommandList);
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
 * @param[in] context Level Zero context handle for the operation
 * @param[in] ze_device Device handle for the target GPU device
 * @param[in] command_queue_group_ordinal Ordinal of the command queue group
 * @param[in] command_queue_index Index within the command queue group
 * @param[out] phCommandQueue Pointer to receive the created command queue handle
 * @param[in] flags Command queue creation flags
 * @return ze_result_t ZE_RESULT_SUCCESS on successful creation, error code otherwise
 */
ze_result_t diagnostic::commandQueueCreate(const ze_context_handle_t contextHandle, ze_device_handle_t zeDevice,
										   const uint32_t commandQueueGroupOrdinal, const uint32_t commandQueueIndex,
										   ze_command_queue_handle_t *phCommandQueue, uint32_t flags)
{
	ze_command_queue_desc_t commandQueueDescription{.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
													.pNext = nullptr,
													.ordinal = commandQueueGroupOrdinal,
													.index = commandQueueIndex,
													.flags = flags,
													.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
													.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL};

	ze_result_t ret;
	ret = zeCommandQueueCreate(contextHandle, zeDevice, &commandQueueDescription, phCommandQueue);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't create command queue: %s\n", l0_error_to_string(ret));
	}
	return ret;
}

/**
 * @brief Configures work group parameters for GPU kernel execution
 *
 * This function calculates optimal work group dimensions and counts based on
 * device compute properties and requested work items. It ensures work group
 * configuration stays within device limits while maximizing GPU utilization.
 *
 * @param[in] device_compute_properties Pointer to device compute properties structure
 * @param[in] total_work_items_requested Total number of work items requested for execution
 * @param[out] workgroup_info Pointer to work group structure to be populated
 * @return uint64_t Actual number of work items that will be executed
 */
uint64_t diagnostic::setWorkgroups(ze_device_compute_properties_t *deviceComputeProperties,
								   const uint64_t totalWorkItemsRequested, struct ZeWorkGroups *workgroupInfo)
{
	auto groupSizeX =
		std::clamp(totalWorkItemsRequested, uint64_t(1), uint64_t(deviceComputeProperties->maxGroupSizeX));
	auto groupSizeY = 1;
	auto groupSizeZ = 1;
	auto groupSize = groupSizeX * groupSizeY * groupSizeZ;

	auto groupCountX = totalWorkItemsRequested / groupSize;
	groupCountX = std::clamp(groupCountX, uint64_t(1), uint64_t(deviceComputeProperties->maxGroupCountX));
	auto remainingItems = totalWorkItemsRequested - groupCountX * groupSize;

	uint64_t groupCountY = remainingItems / (groupCountX * groupSize);
	groupCountY = std::clamp(groupCountY, uint64_t(1), uint64_t(deviceComputeProperties->maxGroupCountY));
	remainingItems = totalWorkItemsRequested - groupCountX * groupCountY * groupSize;

	uint64_t groupCountZ = remainingItems / (groupCountX * groupCountY * groupSize);
	groupCountZ = std::clamp(groupCountZ, uint64_t(1), uint64_t(deviceComputeProperties->maxGroupCountZ));

	auto finalWorkItems = groupCountX * groupCountY * groupCountZ * groupSize;
	remainingItems = totalWorkItemsRequested - finalWorkItems;

	workgroupInfo->group_size_x = static_cast<uint32_t>(groupSizeX);
	workgroupInfo->group_size_y = static_cast<uint32_t>(groupSizeY);
	workgroupInfo->group_size_z = static_cast<uint32_t>(groupSizeZ);
	workgroupInfo->group_count_x = static_cast<uint32_t>(groupCountX);
	workgroupInfo->group_count_y = static_cast<uint32_t>(groupCountY);
	workgroupInfo->group_count_z = static_cast<uint32_t>(groupCountZ);

	return finalWorkItems;
}

/**
 * @brief Loads SPIR-V binary file for GPU kernel execution
 *
 * This function reads a compiled SPIR-V binary file from the resources directory
 * and loads it into memory for module creation. It provides kernel loading
 * capabilities for diagnostic compute operations.
 *
 * @param[in] filePath Path to the SPIR-V binary file relative to kernels directory
 * @param[out] binary_file Pointer to vector that will contain the loaded binary data
 * @return ze_result_t ZE_RESULT_SUCCESS on successful file loading, error code otherwise
 */
ze_result_t diagnostic::loadBinaryFile(const std::string &filePath, std::vector<uint8_t> *binaryFile)
{
	std::string folder = std::string(XPUM_RESOURCES_DIR) + std::string("kernels/");
	if (!fileExists(folder)) {
		ERR("Kernel folder does not exist: %s\n", folder.c_str());
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	std::string absoluteFilePath = folder + filePath;
	std::ifstream stream(absoluteFilePath, std::ios::in | std::ios::binary);

	if (!stream.good()) {
		ERR("Failed to open kernel file: %s\n", absoluteFilePath.c_str());
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	size_t length = 0;
	stream.seekg(0, stream.end);
	length = static_cast<size_t>(stream.tellg());
	stream.seekg(0, stream.beg);
	binaryFile->resize(length);
	stream.read(reinterpret_cast<char *>(binaryFile->data()), length);
	stream.close();

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes command lists on a GPU command queue
 *
 * This function submits a command list to the specified command queue for
 * execution on the GPU. It provides the mechanism to trigger GPU operations
 * that have been recorded in command lists for diagnostic tests.
 *
 * @param[in] hCommandQueue Handle to the command queue for execution
 * @param[in] hCommandList Handle to the command list to execute
 */
ze_result_t diagnostic::commandQueueExecuteCommandLists(ze_command_queue_handle_t hCommandQueue,
														ze_command_list_handle_t hCommandList)
{
	ze_result_t ret = zeCommandQueueExecuteCommandLists(hCommandQueue, 1, &hCommandList, nullptr);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't execute command lists: %s\n", l0_error_to_string(ret));
	}
	return ret;
}

/**
 * @brief Synchronizes with GPU command queue completion
 *
 * This function waits for all commands in the specified command queue to
 * complete execution. It provides synchronization between CPU and GPU
 * operations, ensuring diagnostic tests complete before proceeding.
 *
 * @param[in] hCommandQueue Handle to the command queue to synchronize with
 */
ze_result_t diagnostic::commandQueueSynchronize(ze_command_queue_handle_t hCommandQueue)
{
	ze_result_t ret = zeCommandQueueSynchronize(hCommandQueue, UINT64_MAX);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't synchronize command queue: %s\n", l0_error_to_string(ret));
	}

	return ret;
}

/**
 * @brief Resets a command list for reuse
 *
 * This function resets a command list to its initial state, allowing it to
 * be reused for recording new GPU commands. It provides efficient command
 * list management for iterative diagnostic operations.
 *
 * @param[in] hCommandList Handle to the command list to reset
 */
ze_result_t diagnostic::commandListReset(ze_command_list_handle_t hCommandList)
{
	ze_result_t ret = zeCommandListReset(hCommandList);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't reset command list: %s\n", l0_error_to_string(ret));
	}
	return ret;
}

/**
 * @brief Creates a GPU kernel from a module
 *
 * This function creates a kernel object from a compiled module, allowing
 * specific kernel functions to be executed on the GPU. It provides the
 * foundation for running compute operations in diagnostic tests.
 *
 * @param[in] hModule Handle to the module containing the kernel
 * @param[in] name String name of the kernel function to create
 * @param[out] hKernel Pointer to receive the created kernel handle
 * @return ze_result_t ZE_RESULT_SUCCESS on successful kernel creation, error code otherwise
 */
ze_result_t diagnostic::kernelCreate(ze_module_handle_t hModule, std::string name, ze_kernel_handle_t *hKernel)
{
	ze_kernel_desc_t testFunctionDescription = {};
	testFunctionDescription.stype = ZE_STRUCTURE_TYPE_KERNEL_DESC;
	testFunctionDescription.pNext = nullptr;
	testFunctionDescription.flags = 0;
	testFunctionDescription.pKernelName = name.c_str();

	ze_result_t ret = zeKernelCreate(hModule, &testFunctionDescription, hKernel);
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
 * @param[in] context Level Zero context handle
 * @param[in] ptr Pointer to the device memory to free
 */
ze_result_t diagnostic::memoryFree(const ze_context_handle_t &contextHandle, const void *ptr)
{
	ze_result_t ret = zeMemFree(contextHandle, const_cast<void *>(ptr));
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Error freeing memory: %s\n", l0_error_to_string(ret));
	}

	return ret;
}

/**
 * @brief Configures a kernel function with input and output parameters
 *
 * This function sets up a kernel for execution by creating the kernel object
 * and configuring its arguments with input and output memory buffers.
 * It prepares kernels for diagnostic compute operations.
 *
 * @param[in] module_handle Handle to the module containing the kernel
 * @param[out] function Reference to kernel handle to be created and configured
 * @param[in] name Name of the kernel function to set up
 * @param[in] input Pointer to input data buffer
 * @param[in] output Pointer to output data buffer
 */
ze_result_t diagnostic::setupFunction(ze_module_handle_t moduleHandle, ze_kernel_handle_t &function,
									  const std::string &name, void *input, void *output)
{
	ze_result_t ret = kernelCreate(moduleHandle, name, &function);
	if (ret != ZE_RESULT_SUCCESS) {
		return ret;
	}

	ret = zeKernelSetArgumentValue(function, 0, sizeof(input), &input);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Error setting kernel argument value: %s\n", l0_error_to_string(ret));
		return ret;
	}
	ret = zeKernelSetArgumentValue(function, 1, sizeof(output), &output);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Error setting kernel argument value: %s\n", l0_error_to_string(ret));
		return ret;
	}
	return ret;
}

/**
 * @brief Appends a kernel launch command to a command list
 *
 * This function records a kernel launch operation in the specified command
 * list with the given launch parameters. It provides the mechanism to
 * schedule kernel execution as part of GPU command sequences.
 *
 * @param[in] hCommandList Handle to the command list
 * @param[in] hKernel Handle to the kernel to launch
 * @param[in] pLaunchFuncArgs Pointer to launch function arguments containing work group configuration
 */
void diagnostic::commandListAppendLaunchKernel(ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel,
											   const ze_group_count_t *pLaunchFuncArgs)
{
	ze_result_t ret = zeCommandListAppendLaunchKernel(hCommandList, hKernel, pLaunchFuncArgs, nullptr, 0, nullptr);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Error appending launch kernel: %s\n", l0_error_to_string(ret));
	}
}

/**
 * @brief Executes a GPU kernel with timing and performance measurement
 *
 * This function runs a GPU kernel with the specified work group configuration,
 * measures execution time, and optionally performs performance analysis.
 * It supports both functional testing and performance benchmarking modes.
 *
 * @param[in] command_queue Handle to the command queue for kernel execution
 * @param[in] command_list Handle to the command list containing kernel commands
 * @param[in,out] function Reference to the kernel function to execute
 * @param[in] workgroup_info Work group configuration parameters
 * @param[in] checkOnly Boolean flag: true for functional testing, false for performance measurement
 * @return long double Execution time in nanoseconds, or -1 on error
 */
long double diagnostic::runKernel(ze_command_queue_handle_t commandQueue, ze_command_list_handle_t commandList,
								  ze_kernel_handle_t &function, struct ZeWorkGroups &workgroupInfo, bool checkOnly)
{
	long double timed = 0;

	ze_result_t ret = zeKernelSetGroupSize(function, workgroupInfo.group_size_x, workgroupInfo.group_size_y,
										   workgroupInfo.group_size_z);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Error setting kernel group size: %s\n", l0_error_to_string(ret));
		return -1;
	}

	ze_group_count_t threadGroupDimensions;
	threadGroupDimensions.groupCountX = workgroupInfo.group_count_x;
	threadGroupDimensions.groupCountY = workgroupInfo.group_count_y;
	threadGroupDimensions.groupCountZ = workgroupInfo.group_count_z;
	commandListAppendLaunchKernel(commandList, function, &threadGroupDimensions);

	ret = zeCommandListClose(commandList);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't close command list: %s\n", l0_error_to_string(ret));
		return -1;
	}

	// 1 round is good enough if it is not perf diag
	if (checkOnly == true) {
		commandQueueExecuteCommandLists(commandQueue, commandList);
		commandQueueSynchronize(commandQueue);
		return 0;
	}
	// Consistent with ze_peak
	uint32_t warmupIterations = 5;
	uint32_t iters = 20;
	for (uint32_t i = 0; i < warmupIterations; i++) {
		commandQueueExecuteCommandLists(commandQueue, commandList);
		commandQueueSynchronize(commandQueue);
	}

	std::chrono::high_resolution_clock::time_point timeStart, timeEnd;
	timeStart = std::chrono::high_resolution_clock::now();
	for (uint32_t i = 0; i < iters; i++) {
		commandQueueExecuteCommandLists(commandQueue, commandList);
		commandQueueSynchronize(commandQueue);
	}

	timeEnd = std::chrono::high_resolution_clock::now();
	timed = std::chrono::duration<long double, std::chrono::nanoseconds::period>(timeEnd - timeStart).count();
	commandListReset(commandList);
	return (timed / static_cast<long double>(iters));
}

/**
 * @brief calculate Power Consumption on a device
 *
 * This function calculate Power Consumption test on the specified device using Level Zero APIs.
 *
 * The test:
 * 1. Retrieves device power, power domain count ad related power handles
 * 2. For each power domain, takes two power measurements.
 * 3. calculate summary of all the device and sub device domain power values.
 * 4. Return the maximum of them
 * @param[in] d Pointer to the device information structure containing the device handle and context
 * @param[out] totalPowerValue indicate the power value for the device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful test completion, or an error code on failure
 */
ze_result_t diagnostic::calculatePowerConsumption(devInfo *d, int &totalPowerValue)
{
	ze_result_t ret;
	zes_power_properties_t props = {};
	zes_power_ext_properties_t extProps = {};
	props.pNext = &extProps;
	props.stype = ZES_STRUCTURE_TYPE_POWER_PROPERTIES;
	extProps.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
	zes_power_energy_counter_t snap1, snap2;
	int value;
	uint32_t powerDomainCount;
	zes_pwr_handle_t *powerHandles;
	int currentDevicePowerValueSum = 0;
	int currentSubDevicePowerValueSum = 0;

	power *pwr = d->dev->getPower();
	if (pwr == nullptr) {
		ERR("Error: Power pointer not found.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	powerDomainCount = pwr->getPowerCount();
	powerHandles = pwr->getPowerHandles();
	for (uint32_t i = 0; i < powerDomainCount; ++i) {
		ret = pwr->getProperties(powerHandles[i], &props, &extProps);
		if (ret != ZE_RESULT_SUCCESS) {
			continue;
		}

		if (extProps.domain != ZES_POWER_DOMAIN_PACKAGE)
			continue;

		ret = pwr->getEnergyCounter(powerHandles[i], &snap1);
		if (ret == ZE_RESULT_SUCCESS) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			ret = pwr->getEnergyCounter(powerHandles[i], &snap2);
			if (ret == ZE_RESULT_SUCCESS) {
				value = static_cast<int>(std::ceil(static_cast<double>(snap2.energy - snap1.energy) * 1.0 /
												   static_cast<double>(snap2.timestamp - snap1.timestamp)));
				if (!props.onSubdevice) {
					currentDevicePowerValueSum += value;
				} else {
					currentSubDevicePowerValueSum += value;
				}
			}
		}
	}
	totalPowerValue = std::max(currentDevicePowerValueSum, currentSubDevicePowerValueSum);
	DBG("update peak power value: %d\n", totalPowerValue);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Performs a pcie Bandwidth test on a device.
 *
 * This function executes Command Queue Groups test on the specified device using Level Zero APIs.
 * It loads and executes that performs the command queue lists in each copy Engine Group in iteration way,
 * then measures the pcie bandwidth & Latency.
 * The test:
 * 1. Retrieves device command queue group properties
 * 2. For each group item, creates necessary memory allocations
 * 3. For each group item, sets up command lists and queues
 * 4. For each group item, execute Command Lists in iteration way
 * 5. For each group item, measures pcie Bandwidth & Latency
 * 6. Cleans up resources
 * 7. repeat the same setup and measure for all the group item and
 *    get the total bandwidth & Latency
 * @param[in] d Pointer to the device information structure containing the device handle and context
 * @param[out] totalBandwidth  long double for the device pcie total bandwidth information
 * @param[out] totalLatency long double for the device pcie total latency information
 * @return ze_result_t ZE_RESULT_SUCCESS on successful test completion, or an error code on failure
 */
ze_result_t diagnostic::pcieBandwidthTest(devInfo *d, long double &totalBandwidth, long double &totalLatency)
{
	TRACING();

	uint32_t cmdQueuePropsCount;
	ze_result_t ret;
	ze_context_handle_t context = d->dev->getContext();
	long double tBandwidth = 0.0;
	long double tLatency = 0.0;
	long double allBandwidth = 0.0;
	long double allLatency = 0.0;

	cmdQueuePropsCount = d->dev->getCmdQueuePropsCount();
	if (cmdQueuePropsCount == 0) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	for (uint32_t copyEngineGroupId = 0; copyEngineGroupId < cmdQueuePropsCount; ++copyEngineGroupId) {
		ze_command_list_handle_t commandList;
		ret = commandListCreate(context, d->deviceHdl, copyEngineGroupId, &commandList,
								ZE_COMMAND_LIST_FLAG_EXPLICIT_ONLY);
		if (ret != ZE_RESULT_SUCCESS) {
			return ret;
		}

		ze_command_queue_handle_t commandQueue;

		ret = commandQueueCreate(context, d->deviceHdl, copyEngineGroupId, 0, &commandQueue,
								 ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY);
		if (ret != ZE_RESULT_SUCCESS) {
			return ret;
		}

		std::size_t size = 1 << 28;
		void *deviceBuffer;
		void *hostBuffer;

		ret = memoryAlloc(context, d->deviceHdl, size, 1, &deviceBuffer);
		if (ret != ZE_RESULT_SUCCESS) {
			return ret;
		}
		ret = memoryAllocHost(context, size, 1, &hostBuffer);
		if (ret != ZE_RESULT_SUCCESS) {
			return ret;
		}

		uint32_t nIterations = 500;
		long double totalTimeNsec = 0.0;
		std::size_t elementSize = sizeof(uint8_t);
		std::size_t bufferSize = elementSize * size;
		ret = zeCommandListAppendMemoryCopy(commandList, deviceBuffer, hostBuffer, bufferSize, nullptr, 0, nullptr);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Couldn't append memory copy: %s\n", l0_error_to_string(ret));
			memoryFree(context, deviceBuffer);
			memoryFree(context, hostBuffer);
			return ret;
		}

		ret = zeCommandListClose(commandList);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Couldn't close command list: %s\n", l0_error_to_string(ret));
			memoryFree(context, deviceBuffer);
			memoryFree(context, hostBuffer);
			return ret;
		}

		std::chrono::high_resolution_clock::time_point timeStart, timeEnd;
		timeStart = std::chrono::high_resolution_clock::now();
		for (uint32_t i = 0; i < nIterations; i++) {
			commandQueueExecuteCommandLists(commandQueue, commandList);
			commandQueueSynchronize(commandQueue);
		}
		timeEnd = std::chrono::high_resolution_clock::now();
		totalTimeNsec =
			std::chrono::duration<long double, std::chrono::nanoseconds::period>(timeEnd - timeStart).count();

		ret = zeCommandListDestroy(commandList);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Error destroying command list: %s\n", l0_error_to_string(ret));
		}
		ret = zeCommandQueueDestroy(commandQueue);
		memoryFree(context, deviceBuffer);
		memoryFree(context, hostBuffer);
		calculateBandwidthLatency(totalTimeNsec, static_cast<long double>(size * nIterations), tBandwidth, tLatency,
								  nIterations);
		allBandwidth = allBandwidth + tBandwidth;
		allLatency = allLatency + tLatency;
	}
	totalBandwidth = allBandwidth;
	totalLatency = allLatency;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Performs a memory test on a device to evaluate memory allocation related features based on the number Of
 * dispatch This function. It loads and executes a shader program (.spv file) that performs intensive integer
 * calculations based on each dispatch, then test memory allocation related features.
 *
 * The test for each dispatch:
 * 1. Create the kernel module, group size and arguments
 * 2. Sets up command lists and queues
 * 3. Creates necessary memory allocations and fills the values and copy the memory
 * 4. Executes command lists and queues
 * 5. Cleans up resources
 *
 * @param[in] d Pointer to the device information structure containing the device handle and context
 * @param[in] module, a specific ze module handle
 * @param[in] srcAllocations pointer to a source uint8_t vector
 * @param[in] dstAllocations pointer to a destination uint8_t vector
 * @param[in] dataOut reference to a data out uint8_t vector
 * @param[in] testKernelNames, reference to a related kernel module name
 * @param[in] numberOfDispatch, a uint64_t total dispatch number
 * @param[in] allocationCount, a uint64_t one Case allocation count
 * @param[in] context, a device application context
 * @return ze_result_t ZE_RESULT_SUCCESS on successful test completion, or an error code on failure
 */
ze_result_t diagnostic::dispatchKernelsForMemoryTest(
	devInfo *d, ze_module_handle_t module, std::vector<uint8_t *> srcAllocations, std::vector<uint8_t *> dstAllocations,
	std::vector<std::vector<uint8_t>> &dataOut, const std::vector<std::string> &testKernelNames,
	uint64_t numberOfDispatch, uint64_t allocationCount, ze_context_handle_t context)
{
	ze_result_t ret;
	std::vector<ze_kernel_handle_t> testFunctions;
	uint32_t workgroupSizeX = 8;
	uint8_t initValue2 = 0xAA; // 1010 1010
	uint8_t initValue3 = 0x55; // 0101 0101

	ze_command_list_handle_t commandList = nullptr;
	ret = commandListCreate(context, d->deviceHdl, 0, &commandList, 0);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Failed to create command list : %s\n", l0_error_to_string(ret));
		return ret;
	}
	for (uint64_t dispatchId = 0; dispatchId < numberOfDispatch; dispatchId++) {
		uint8_t *srcAllocation = srcAllocations[dispatchId];
		uint8_t *dstAllocation = dstAllocations[dispatchId];

		ze_kernel_handle_t testFunction = nullptr;
		kernelCreate(module, testKernelNames[dispatchId], &testFunction);
		ret = zeKernelSetGroupSize(testFunction, workgroupSizeX, 1, 1);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Failed to set kernel group size: %s\n", l0_error_to_string(ret));
			return ret;
		}
		ret = zeKernelSetArgumentValue(testFunction, 0, sizeof(srcAllocation), &srcAllocation);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Failed to set kernel argument value : %s\n", l0_error_to_string(ret));
			return ret;
		}
		ret = zeKernelSetArgumentValue(testFunction, 1, sizeof(dstAllocation), &dstAllocation);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Failed to set kernel argument value : %s\n", l0_error_to_string(ret));
			return ret;
		}
		uint32_t groupCountX = static_cast<uint32_t>(allocationCount / workgroupSizeX);
		ze_group_count_t threadGroupDimensions = {groupCountX, 1, 1};

		ret = zeCommandListAppendMemoryFill(commandList, srcAllocation, &initValue2, sizeof(uint8_t),
											allocationCount * sizeof(uint8_t), nullptr, 0, nullptr);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Failed to fill command list append memory : %s\n", l0_error_to_string(ret));
			return ret;
		}

		ret = zeCommandListAppendBarrier(commandList, nullptr, 0, nullptr);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Failed to append command list barrier: %s\n", l0_error_to_string(ret));
			return ret;
		}

		ret = zeCommandListAppendMemoryFill(commandList, dstAllocation, &initValue3, sizeof(uint8_t),
											allocationCount * sizeof(uint8_t), nullptr, 0, nullptr);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Failed to fill command list append memory : %s\n", l0_error_to_string(ret));
			return ret;
		}

		ret = zeCommandListAppendBarrier(commandList, nullptr, 0, nullptr);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Failed to append command list barrier : %s\n", l0_error_to_string(ret));
			return ret;
		}

		commandListAppendLaunchKernel(commandList, testFunction, &threadGroupDimensions);
		ret = zeCommandListAppendBarrier(commandList, nullptr, 0, nullptr);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Failed to append command list barrier: %s\n", l0_error_to_string(ret));
			return ret;
		}

		ret = zeCommandListAppendMemoryCopy(commandList, dataOut[dispatchId].data(), dstAllocation,
											dataOut[dispatchId].size() * sizeof(uint8_t), nullptr, 0, nullptr);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Failed to copy command list append memory : %s\n", l0_error_to_string(ret));
			return ret;
		}

		ret = zeCommandListAppendBarrier(commandList, nullptr, 0, nullptr);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Failed to append command list barrier : %s\n", l0_error_to_string(ret));
			return ret;
		}

		testFunctions.push_back(testFunction);
	}
	ret = zeCommandListClose(commandList);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Failed to close command list : %s\n", l0_error_to_string(ret));
		return ret;
	}

	ze_command_queue_handle_t commandQueue;
	ret = commandQueueCreate(context, d->deviceHdl, 0, 0, &commandQueue, ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Failed to create command queue : %s\n", l0_error_to_string(ret));
		return ret;
	}

	commandQueueExecuteCommandLists(commandQueue, commandList);
	commandQueueSynchronize(commandQueue);
	ret = zeCommandQueueDestroy(commandQueue);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Failed to destroy command queue  : %s\n", l0_error_to_string(ret));
		return ret;
	}

	ret = zeCommandListDestroy(commandList);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Failed to destroy command list : %s\n", l0_error_to_string(ret));
		return ret;
	}

	for (uint64_t dispatchId = 0; dispatchId < testFunctions.size(); dispatchId++) {
		ret = zeKernelDestroy(testFunctions[dispatchId]);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Failed to run kernel destroy : %s\n", l0_error_to_string(ret));
			return ret;
		}
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief performance memory allocation
 *
 * This function performance memory allocation test on the specified device using Level Zero APIs.
 *
 * The test:
 * 1. Retrieves target test memory size from device maxMemAllocSize and real host memory size.
 * 2. based on each memory usage, allocation sizes, and memory Types, calculate the requested allocation size and number
 * Of dispatch based on max allocation size
 * 3. allocate the memory for each dispatch's memory Input, memory output and setup the test Kernel name and module.
 * 4. run the memory test for all the dispatch and settings by using spv file.
 * @param[in] d Pointer to the device information structure containing the device handle and context
 * @return ze_result_t ZE_RESULT_SUCCESS on successful test completion, or an error code on failure
 */
ze_result_t diagnostic::peformanceMemoryAllocation(devInfo *d)
{
	ze_result_t ret;
	uint64_t oneMB = 1024UL * 1024UL;
	uint64_t oneGB = 1UL * 1024UL * 1024UL * 1024UL;
	uint32_t numberOfKernelArgs = 2;
	uint32_t numberOfKernelsInModule = 10;

	std::vector<float> memoryUseLevels = {0.1f, 0.5f, 0.9f, 1.0f};
	std::vector<uint64_t> allocateSizes = {oneMB, oneGB};
	std::vector<std::string> memoryTypes = {"DEVICE", "SHARED"};
	ze_device_properties_t zeDevProp = {};
	ze_context_handle_t context = d->dev->getContext();

	for (auto &memoryUse : memoryUseLevels)
		for (auto &allocateSize : allocateSizes)
			for (auto &memoryType : memoryTypes) {
				ret = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
				if (ret != ZE_RESULT_SUCCESS) {
					ERR("Failed to get device properties : %s\n", l0_error_to_string(ret));
					return ret;
				}
				uint64_t targetTestMemorySize = zeDevProp.maxMemAllocSize;
				uint64_t hostMemorySize;

				if (CHECKHOSTMEMORYSIZE(&hostMemorySize)) {
					if (targetTestMemorySize > hostMemorySize) {
						targetTestMemorySize = hostMemorySize;
					}
				}
				uint64_t maxAllocationSize =
					static_cast<uint64_t>(static_cast<float>(targetTestMemorySize) * memoryUse);
				uint64_t oneCaseRequestedAllocationSize = allocateSize * numberOfKernelArgs;

				if (oneCaseRequestedAllocationSize > maxAllocationSize) {
					oneCaseRequestedAllocationSize = maxAllocationSize;
				}
				std::size_t allocationCount = oneCaseRequestedAllocationSize / (numberOfKernelArgs * sizeof(uint8_t));
				std::uint64_t numberOfDispatch = maxAllocationSize / oneCaseRequestedAllocationSize;
				std::vector<uint8_t *> inputAllocations;
				std::vector<uint8_t *> outputAllocations;
				std::vector<std::vector<uint8_t>> dataOutVector;
				std::vector<std::string> testKernelNames;

				for (uint64_t dispatchId = 0; dispatchId < numberOfDispatch; dispatchId++) {
					uint8_t *inputAllocation;
					uint8_t *outputAllocation;
					if (memoryType == "HOST") {
						void *memoryInput = nullptr;
						memoryAllocHost(context, allocationCount, 8, &memoryInput);
						inputAllocation = (uint8_t *)memoryInput;
						void *memoryOutput = nullptr;
						memoryAllocHost(context, allocationCount, 8, &memoryOutput);
						outputAllocation = (uint8_t *)memoryOutput;

					} else if (memoryType == "DEVICE") {
						void *memoryInput = nullptr;
						memoryAlloc(context, d->deviceHdl, allocationCount, 8, &memoryInput);
						inputAllocation = (uint8_t *)memoryInput;
						void *memoryOutput = nullptr;
						memoryAlloc(context, d->deviceHdl, allocationCount, 8, &memoryOutput);
						outputAllocation = (uint8_t *)memoryOutput;
					} else {
						void *memoryInput = nullptr;
						memoryAllocShared(context, d->deviceHdl, allocationCount, 8, &memoryInput);
						inputAllocation = (uint8_t *)memoryInput;

						void *memoryOutput = nullptr;
						memoryAllocShared(context, d->deviceHdl, allocationCount, 8, &memoryOutput);
						outputAllocation = (uint8_t *)memoryOutput;
					}
					inputAllocations.push_back(inputAllocation);
					outputAllocations.push_back(outputAllocation);
					std::vector<uint8_t> dataOut(allocationCount, 0);
					dataOutVector.push_back(dataOut);
					std::string kernelName;
					kernelName = "test_device_memory" + std::to_string((dispatchId % numberOfKernelsInModule) + 1);
					testKernelNames.push_back(kernelName);
				}

				std::vector<uint8_t> binaryFile;
				ret = loadBinaryFile("test_multiple_memory_allocations.spv", &binaryFile);
				ze_module_handle_t moduleHandle = nullptr;
				ret = moduleCreate(context, d->deviceHdl, binaryFile, &moduleHandle);
				if (ret != ZE_RESULT_SUCCESS) {
					ERR("Failed to run memory allocation test : %s\n", l0_error_to_string(ret));
					for (auto eachAllocation : inputAllocations) {
						memoryFree(context, eachAllocation);
					}
					for (auto eachAllocation : outputAllocations) {
						memoryFree(context, eachAllocation);
					}
					moduleDestroy(moduleHandle);
					return ret;
				}

				ret = dispatchKernelsForMemoryTest(d, moduleHandle, inputAllocations, outputAllocations, dataOutVector,
												   testKernelNames, numberOfDispatch, allocationCount, context);
				if (ret != ZE_RESULT_SUCCESS) {
					ERR("Failed to run dispatch kernels for memory test : %s\n", l0_error_to_string(ret));
					for (auto eachAllocation : inputAllocations) {
						memoryFree(context, eachAllocation);
					}
					for (auto eachAllocation : outputAllocations) {
						memoryFree(context, eachAllocation);
					}
					moduleDestroy(moduleHandle);
					return ret;
				}
				for (auto eachAllocation : inputAllocations) {
					memoryFree(context, eachAllocation);
				}
				for (auto eachAllocation : outputAllocations) {
					memoryFree(context, eachAllocation);
				}
				moduleDestroy(moduleHandle);
			}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief memory error test
 *
 * This function run memory error test on the specified device using Level Zero APIs.
 *
 * The test:
 * 1. Retrieves target test memory size from device max mem allocation size and real host memory size.
 * 2. based on the memory usage, allocation sizes, calculate the requested allocation size and number
 * Of dispatch based on max allocation size
 * 3. allocate the memory for each dispatch's memory Input, memory output and setup the test Kernel name and module.
 * 4. run the memory test for all the dispatch and settings by using spv file.
 * 5. check if the output memory content correct or not.
 * @param[in] d Pointer to the device information structure containing the device handle and context
 * @return ze_result_t ZE_RESULT_SUCCESS on successful test completion, or an error code on failure
 */
ze_result_t diagnostic::memoryErrorTest(devInfo *d, int &errorCount)
{
	ze_result_t ret;
	constexpr uint64_t oneMB = 1024UL * 1024UL;
	constexpr uint32_t numberOfKernelArgs = 2;
	constexpr uint32_t numberOfKernelsInModule = 10;
	constexpr uint8_t initValue = 0xAA;
	constexpr float memoryUse = 0.9f;

	ze_device_properties_t zeDevProp = {};
	ze_context_handle_t context = d->dev->getContext();

	ret = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties : %s\n", l0_error_to_string(ret));
		return ret;
	}
	// Retrieves target test memory size from device max mem allocation size and real host memory size
	uint64_t targetTestMemorySize = zeDevProp.maxMemAllocSize;
	uint64_t hostMemorySize;

	if (CHECKHOSTMEMORYSIZE(&hostMemorySize)) {
		if (targetTestMemorySize > hostMemorySize) {
			targetTestMemorySize = hostMemorySize;
		}
	}
	// based on the memory usage, allocation sizes, calculate the requested allocation size and number Of dispatch based
	// on max allocation size
	uint64_t maxAllocationSize = static_cast<uint64_t>(static_cast<float>(targetTestMemorySize) * memoryUse);

	uint64_t oneCaseRequestedAllocationSize = oneMB * numberOfKernelArgs;

	if (oneCaseRequestedAllocationSize > maxAllocationSize) {
		oneCaseRequestedAllocationSize = maxAllocationSize;
	}
	// allocate the memory for each dispatch's memory Input, memory output and setup the test Kernel name and module.
	std::size_t allocationCount =
		static_cast<std::size_t>(oneCaseRequestedAllocationSize / (numberOfKernelArgs * sizeof(uint8_t)));
	std::uint64_t numberOfDispatch = maxAllocationSize / oneCaseRequestedAllocationSize;
	std::vector<uint8_t *> inputAllocations;
	std::vector<uint8_t *> outputAllocations;
	std::vector<std::vector<uint8_t>> dataOutVector;
	std::vector<std::string> testKernelNames;

	for (uint64_t dispatchId = 0; dispatchId < numberOfDispatch; dispatchId++) {
		uint8_t *inputAllocation;
		uint8_t *outputAllocation;
		void *memoryInput = nullptr;
		memoryAlloc(context, d->deviceHdl, allocationCount, 8, &memoryInput);
		inputAllocation = (uint8_t *)memoryInput;
		void *memoryOutput = nullptr;
		memoryAlloc(context, d->deviceHdl, allocationCount, 8, &memoryOutput);
		outputAllocation = (uint8_t *)memoryOutput;

		inputAllocations.push_back(inputAllocation);
		outputAllocations.push_back(outputAllocation);
		std::vector<uint8_t> dataOut(allocationCount, 0);
		dataOutVector.push_back(dataOut);
		std::string kernelName;
		kernelName = "test_device_memory" + std::to_string((dispatchId % numberOfKernelsInModule) + 1);
		testKernelNames.push_back(kernelName);
	}
	// run the memory test for all the dispatch and settings by using spv file
	std::vector<uint8_t> binaryFile;
	ret = loadBinaryFile("test_multiple_memory_allocations.spv", &binaryFile);
	ze_module_handle_t moduleHandle = nullptr;
	ret = moduleCreate(context, d->deviceHdl, binaryFile, &moduleHandle);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Failed to run memory allocation test : %s\n", l0_error_to_string(ret));
		for (auto eachAllocation : inputAllocations) {
			memoryFree(context, eachAllocation);
		}
		for (auto eachAllocation : outputAllocations) {
			memoryFree(context, eachAllocation);
		}
		moduleDestroy(moduleHandle);
		return ret;
	}
	ret = dispatchKernelsForMemoryTest(d, moduleHandle, inputAllocations, outputAllocations, dataOutVector,
									   testKernelNames, numberOfDispatch, allocationCount, context);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Failed to run dispatch kernels for memory test : %s\n", l0_error_to_string(ret));
		for (auto eachAllocation : inputAllocations) {
			memoryFree(context, eachAllocation);
		}
		for (auto eachAllocation : outputAllocations) {
			memoryFree(context, eachAllocation);
		}
		moduleDestroy(moduleHandle);
		return ret;
	}
	for (auto eachAllocation : inputAllocations) {
		memoryFree(context, eachAllocation);
	}
	for (auto eachAllocation : outputAllocations) {
		memoryFree(context, eachAllocation);
	}
	moduleDestroy(moduleHandle);

	// check if the output memory content correct or not
	errorCount = 0;
	for (auto eachDataOut : dataOutVector) {
		for (uint32_t i = 0; i < allocationCount; i++) {
			if (initValue != eachDataOut[i]) {
				errorCount += 1;
			}
		}
	}
	if (errorCount > 0) {
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief  free all buffer and module handle related resources
 *
 * This function properly free all buffer and module handle related resources,
 * and handles. It provides cleanup functionality for diagnostic operations using GPU modules.
 *
 * @param[in] moduleHandle Handle to the module to be destroyed
 * @param[in] inputBuf pointer to the buffer to be freed
 * @param[in] outputBuf pointer to the buffer to be freed
 */
void diagnostic::freeResource(ze_context_handle_t context, void *inputBuf, void *outputBuf,
							  ze_module_handle_t moduleHandle)
{
	memoryFree(context, inputBuf);
	memoryFree(context, outputBuf);
	moduleDestroy(moduleHandle);
}

/**
 * @brief calculate Gbps for workGroups
 *
 * This function calculate Gbps for workGroups by running a GPU kernel with the specified work group configuration,
 * measures execution time, and optionally performs performance analysis.
 *
 * @param[in] commandQueue Handle to the command queue for kernel execution
 * @param[in] commandList Handle to the command list containing kernel commands
 * @param[in] function1 Reference to the kernel function1 to execute
 * @param[in] function2 Reference to the kernel function2 to execute
 * @param[in] numItems number of work items that will be executed
 * @param[in] factor  size number that will be executed
 * @param[in] zeComputeProp Reference to device compute properties structure
 * @param[in] workgroupInfo Reference to work group configuration parameters
 * @return long double Execution time in seconds
 */
long double diagnostic::calculateGbpsForWorkGroups(ze_command_queue_handle_t commandQueue,
												   ze_command_list_handle_t commandList, ze_kernel_handle_t &function1,
												   ze_kernel_handle_t &function2, uint64_t numItems, uint64_t factor,
												   ze_device_compute_properties_t &zeComputeProp,
												   struct ZeWorkGroups &workgroupInfo)
{
	long double timedLo, timedGo, timed, gbps;
	uint64_t tempGlobalSize;

	timed = 0;
	timedLo = 0;
	timedGo = 0;
	tempGlobalSize = (numItems / factor / 16);
	tempGlobalSize = setWorkgroups(&zeComputeProp, tempGlobalSize, &workgroupInfo);
	timedLo = runKernel(commandQueue, commandList, function1, workgroupInfo, false);
	timedGo = runKernel(commandQueue, commandList, function2, workgroupInfo, false);
	timed = (timedLo < timedGo) ? timedLo : timedGo;
	gbps = calculateGbps(timed, static_cast<long double>(numItems * sizeof(float)));
	return gbps;
}

/**
 * @brief bandwidth test
 *
 * This function run memory bandwidth test test on the specified device using Level Zero APIs.
 *
 * The test:
 * 1. Retrieves target copy engine group count.
 * 2. based on the copy engine group count, setup the command list and command queue to run the computation test using
 * the ze_global_bw.spv file.
 * 3. calculate the max the bandwidth for each copy engine group item.
 * 4. check if the computation test successful or not
 * 5. calculate and return the final test result
 * @param[in] d Pointer to the device information structure containing the device handle and context
 * @param[out] totalGbps reference to the final bandwidth result
 * @return ze_result_t ZE_RESULT_SUCCESS on successful test completion, or an error code on failure
 */
ze_result_t diagnostic::memoryBandwidthTest(devInfo *d, std::vector<long double> &totalGbps)
{
	TRACING();
	uint32_t cmdQueuePropsCount;
	ze_result_t ret;

	ze_context_handle_t context = d->dev->getContext();

	cmdQueuePropsCount = d->dev->getCmdQueuePropsCount();
	if (cmdQueuePropsCount == 0) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	for (uint32_t copyEngineGroupId = 0; copyEngineGroupId < cmdQueuePropsCount; ++copyEngineGroupId) {

		struct ZeWorkGroups workgroupInfo;
		long double gbps, allGbps = 0.0;

		ze_device_properties_t zeDevProp = {};
		ze_device_compute_properties_t zeComputeProp = {};

		ret = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Failed to get device properties: 0x%X (%s)\n", ret, l0_error_to_string(ret));
			return ret;
		}

		ret = d->dev->getComputeProps(d->deviceHdl, &zeComputeProp);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Failed to get device properties: 0x%X (%s)\n", ret, l0_error_to_string(ret));
			return ret;
		}

		std::vector<uint8_t> binaryFile;
		ze_module_handle_t moduleHandle;
		ret = loadBinaryFile("ze_global_bw.spv", &binaryFile);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Failed to load binary file: %s\n", l0_error_to_string(ret));
			return ret;
		}

		moduleCreate(context, d->deviceHdl, binaryFile, &moduleHandle);

		uint64_t maxItems = zeDevProp.maxMemAllocSize / sizeof(float) / 2;
		uint64_t numItems = std::min(maxItems, (uint64_t)(1 << 29));
		uint64_t base = (uint64_t)zeComputeProp.maxGroupSizeX * 16 * 16;
		numItems = (numItems / base) * base;

		std::vector<float> arr(static_cast<uint32_t>(numItems));
		for (uint32_t i = 0; i < numItems; i++) {
			arr[i] = static_cast<float>(i);
		}

		numItems = setWorkgroups(&zeComputeProp, numItems, &workgroupInfo);

		void *inputBuf;
		ret = memoryAlloc(context, d->deviceHdl, static_cast<size_t>((numItems * sizeof(float))), 1, &inputBuf);
		if (ret != ZE_RESULT_SUCCESS) {
			moduleDestroy(moduleHandle);
			return ret;
		}

		void *outputBuf;
		ret = memoryAlloc(context, d->deviceHdl, static_cast<size_t>((numItems * sizeof(float))), 1, &outputBuf);
		if (ret != ZE_RESULT_SUCCESS) {
			memoryFree(context, inputBuf);
			moduleDestroy(moduleHandle);
			return ret;
		}

		ze_command_list_handle_t copyCommandList;
		ret = commandListCreate(context, d->deviceHdl, copyEngineGroupId, &copyCommandList, 0);
		if (ret != ZE_RESULT_SUCCESS) {
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ze_command_queue_handle_t copyCommandQueue;
		commandQueueCreate(context, d->deviceHdl, copyEngineGroupId, 0, &copyCommandQueue, 0);
		if (ret != ZE_RESULT_SUCCESS) {
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ret = zeCommandListAppendMemoryCopy(copyCommandList, inputBuf, arr.data(), (arr.size() * sizeof(float)),
											nullptr, 0, nullptr);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Couldn't append memory copy: %s\n", l0_error_to_string(ret));
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ret = zeCommandListAppendBarrier(copyCommandList, nullptr, 0, nullptr);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Couldn't append barrier: %s\n", l0_error_to_string(ret));
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ret = zeCommandListClose(copyCommandList);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Couldn't close command list: %s\n", l0_error_to_string(ret));
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		commandQueueExecuteCommandLists(copyCommandQueue, copyCommandList);
		commandQueueSynchronize(copyCommandQueue);
		commandListReset(copyCommandList);

		ze_command_list_handle_t commandList;
		ret = commandListCreate(context, d->deviceHdl, 0, &commandList, 0);
		if (ret != ZE_RESULT_SUCCESS) {
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ze_command_queue_handle_t commandQueue;
		ret = commandQueueCreate(context, d->deviceHdl, 0, 0, &commandQueue, 0);
		if (ret != ZE_RESULT_SUCCESS) {
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ze_kernel_handle_t localOffsetV1;
		ze_kernel_handle_t localOffsetV2;
		ze_kernel_handle_t localOffsetV4;
		ze_kernel_handle_t localOffsetV8;
		ze_kernel_handle_t localOffsetV16;
		ze_kernel_handle_t globalOffsetV1;
		ze_kernel_handle_t globalOffsetV2;
		ze_kernel_handle_t globalOffsetV4;
		ze_kernel_handle_t globalOffsetV8;
		ze_kernel_handle_t globalOffsetV16;

		setupFunction(moduleHandle, localOffsetV1, "global_bandwidth_v1_local_offset", inputBuf, outputBuf);
		setupFunction(moduleHandle, globalOffsetV1, "global_bandwidth_v1_global_offset", inputBuf, outputBuf);
		setupFunction(moduleHandle, localOffsetV2, "global_bandwidth_v2_local_offset", inputBuf, outputBuf);
		setupFunction(moduleHandle, globalOffsetV2, "global_bandwidth_v2_global_offset", inputBuf, outputBuf);
		setupFunction(moduleHandle, localOffsetV4, "global_bandwidth_v4_local_offset", inputBuf, outputBuf);
		setupFunction(moduleHandle, globalOffsetV4, "global_bandwidth_v4_global_offset", inputBuf, outputBuf);
		setupFunction(moduleHandle, localOffsetV8, "global_bandwidth_v8_local_offset", inputBuf, outputBuf);
		setupFunction(moduleHandle, globalOffsetV8, "global_bandwidth_v8_global_offset", inputBuf, outputBuf);
		setupFunction(moduleHandle, localOffsetV16, "global_bandwidth_v16_local_offset", inputBuf, outputBuf);
		setupFunction(moduleHandle, globalOffsetV16, "global_bandwidth_v16_global_offset", inputBuf, outputBuf);

		gbps = calculateGbpsForWorkGroups(commandQueue, commandList, localOffsetV1, globalOffsetV1, numItems, 1,
										  zeComputeProp, workgroupInfo);
		allGbps = std::max(allGbps, gbps);

		gbps = calculateGbpsForWorkGroups(commandQueue, commandList, localOffsetV2, globalOffsetV2, numItems, 2,
										  zeComputeProp, workgroupInfo);
		allGbps = std::max(allGbps, gbps);

		gbps = calculateGbpsForWorkGroups(commandQueue, commandList, localOffsetV4, globalOffsetV4, numItems, 4,
										  zeComputeProp, workgroupInfo);
		allGbps = std::max(allGbps, gbps);

		gbps = calculateGbpsForWorkGroups(commandQueue, commandList, localOffsetV8, globalOffsetV8, numItems, 8,
										  zeComputeProp, workgroupInfo);
		allGbps = std::max(allGbps, gbps);

		gbps = calculateGbpsForWorkGroups(commandQueue, commandList, localOffsetV16, globalOffsetV16, numItems, 16,
										  zeComputeProp, workgroupInfo);
		allGbps = std::max(allGbps, gbps);

		ret = zeKernelDestroy(localOffsetV1);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Error destroying kernel: %s\n", l0_error_to_string(ret));
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ret = zeKernelDestroy(globalOffsetV1);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Error destroying kernel: %s\n", l0_error_to_string(ret));
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ret = zeKernelDestroy(localOffsetV2);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Error destroying kernel: %s\n", l0_error_to_string(ret));
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ret = zeKernelDestroy(globalOffsetV2);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Error destroying kernel: %s\n", l0_error_to_string(ret));
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ret = zeKernelDestroy(localOffsetV4);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Error destroying kernel: %s\n", l0_error_to_string(ret));
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ret = zeKernelDestroy(globalOffsetV4);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Error destroying kernel: %s\n", l0_error_to_string(ret));
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ret = zeKernelDestroy(localOffsetV8);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Error destroying kernel: %s\n", l0_error_to_string(ret));
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ret = zeKernelDestroy(globalOffsetV8);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Error destroying kernel: %s\n", l0_error_to_string(ret));
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ret = zeKernelDestroy(localOffsetV16);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Error destroying kernel: %s\n", l0_error_to_string(ret));
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ret = zeKernelDestroy(globalOffsetV16);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Error destroying kernel: %s\n", l0_error_to_string(ret));
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ret = zeCommandListDestroy(commandList);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Error destroying command list: %s\n", l0_error_to_string(ret));
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ret = zeCommandQueueDestroy(commandQueue);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Error destroying command queue: %s\n", l0_error_to_string(ret));
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ret = zeCommandListDestroy(copyCommandList);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Error destroying copy command list: %s\n", l0_error_to_string(ret));
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}

		ret = zeCommandQueueDestroy(copyCommandQueue);
		if (ret != ZE_RESULT_SUCCESS) {
			ERR("Error destroying copy command queue: %s\n", l0_error_to_string(ret));
			freeResource(context, inputBuf, outputBuf, moduleHandle);
			return ret;
		}
		freeResource(context, inputBuf, outputBuf, moduleHandle);
		if (allGbps <= 0) {
			return ZE_RESULT_ERROR_UNKNOWN;
		} else {
			totalGbps.push_back(allGbps);
		}
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Gets available tests within a diagnostic test suite
 *
 * This function retrieves all individual diagnostic tests available within
 * a specific test suite, providing detailed information about test indices
 * and names for comprehensive hardware validation capabilities.
 *
 * @param[in] testSuite Handle to the specific diagnostic test suite
 * @return int Number of test suites available, or 0 on error
 */
int diagnostic::getTests(zes_diag_handle_t testSuite)
{
	uint32_t testCount = 0;
	// Get tests within the diagnostic test suite
	ze_result_t result = zesDiagnosticsGetTests(testSuite, &testCount, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get diagnostic tests count: 0x%X (%s)\n", result, l0_error_to_string(result));
		return 0;
	}

	std::vector<zes_diag_test_t> tests(testCount);
	result = zesDiagnosticsGetTests(testSuite, &testCount, tests.data());
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get diagnostic tests: 0x%X (%s)\n", result, l0_error_to_string(result));
		return 0;
	}

	DBG("    - Test Suite has %d tests:\n", testCount);

	for (const auto &test : tests) {
		DBG("      - Test Index: %d\n", test.index);
		DBG("      - Test Name: %s\n", test.name);
	}
	return testSuiteCount;
}

/**
 * @brief Executes diagnostic tests within a test suite
 *
 * This function runs the specified number of diagnostic tests within a test suite,
 * performing comprehensive hardware validation and health checking to ensure
 * device functionality and identify potential issues.
 *
 * @param[in] testSuite Handle to the specific diagnostic test suite
 * @param[in] testCount Number of tests to execute within the suite
 * @return ze_result_t ZE_RESULT_SUCCESS on successful test execution, error code otherwise
 */
ze_result_t diagnostic::runTests(zes_diag_handle_t testSuite, uint32_t testCount)
{
	// Run diagnostic tests
	ze_result_t result = zesDiagnosticsRunTests(testSuite, 0, testCount, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to run diagnostic tests: 0x%X (%s)\n", result, l0_error_to_string(result));
	} else {
		DBG("    - Diagnostic tests run successfully.\n");
	}

	return result;
}

/**
 * @brief Performs comprehensive diagnostic runtime operations
 *
 * This function executes a complete diagnostic cycle including test suite
 * enumeration, property retrieval, test discovery, and test execution for
 * thorough hardware validation and device health assessment.
 *
 * @param[in] device Handle to the device for diagnostic operations
 * @return ze_result_t ZE_RESULT_SUCCESS on successful execution, error code otherwise
 */
ze_result_t diagnostic::zesRun(zes_device_handle_t device)
{
	ze_result_t result = enumDiag(device);
	if (result != ZE_RESULT_SUCCESS)
		return result;

	// Run diagnostic tests for each test suite
	for (uint32_t i = 0; i < testSuiteCount; i++) {
		zes_diag_handle_t testSuite = testSuites[i];
		getProperties(testSuite);
		uint32_t testCount = getTests(testSuite);
		runTests(testSuite, testCount);
	}
	return result;
}
