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

#include "diagnostic.h"
#include <device.h>
#include <vector>

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
 * @param device Handle to the Level Zero Sysman device
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
 * @param testSuite Handle to the specific diagnostic test suite
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
 * @param period Execution time period in appropriate units
 * @param total_gbps Total throughput measurement
 * @return long double Calculated bandwidth in GBPS, or maximum value if period is near zero
 */
long double calculateGbps(long double period, long double total_gbps)
{
	constexpr auto ep = std::numeric_limits<long double>::epsilon();
	if ((period * 10e3) > (ep * 10e3))
		return total_gbps / period;
	else {
		return std::numeric_limits<long double>::max();
	}
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
 * @param d Pointer to the device information structure containing the device handle and context
 * @return ze_result_t ZE_RESULT_SUCCESS on successful test completion, or an error code on failure
 */
ze_result_t diagnostic::computationTest(devInfo *d, long double &out_gflops)
{
	TRACING();
	ze_device_properties_t zeDevProp = {};
	ze_device_compute_properties_t zeComputeProp = {};
	ze_result_t result;
	ze_context_handle_t context = d->dev->getContext();
	float input_value = 1.3f;
	bool checkOnly = false;
	std::vector<long double> all_gflops;

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
	ze_result_t ret = d->dev->loadBinaryFile("ze_sp_compute.spv", &binary_file);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Failed to load binary file: %s\n", l0_error_to_string(ret));
		return ret;
	}
	ze_module_handle_t module_handle;
	ret = d->dev->moduleCreate(context, d->deviceHdl, binary_file, &module_handle);
	if (ret != ZE_RESULT_SUCCESS) {
		return ret;
	}

	uint64_t max_work_items = (uint64_t)zeDevProp.numSlices * zeDevProp.numSubslicesPerSlice *
							  zeDevProp.numEUsPerSubslice * zeComputeProp.maxGroupCountX * 2048;
	uint64_t max_number_of_allocated_items = zeDevProp.maxMemAllocSize / sizeof(float);
	uint64_t number_of_work_items = std::min(max_number_of_allocated_items, (max_work_items * sizeof(float)));
	ZeWorkGroups workgroup_info;
	number_of_work_items = d->dev->setWorkgroups(&zeComputeProp, number_of_work_items, &workgroup_info);

	void *device_input_value;
	ret = d->dev->memoryAlloc(context, d->deviceHdl, sizeof(float), 1, &device_input_value);
	if (ret != ZE_RESULT_SUCCESS) {
		d->dev->moduleDestroy(module_handle);
		return ret;
	}

	void *device_output_buffer;
	ret = d->dev->memoryAlloc(context, d->deviceHdl, static_cast<std::size_t>((number_of_work_items * sizeof(float))),
							  1, &device_output_buffer);
	if (ret != ZE_RESULT_SUCCESS) {
		d->dev->memoryFree(context, device_input_value);
		d->dev->moduleDestroy(module_handle);
		return ret;
	}

	ze_command_list_handle_t command_list;
	ret = d->dev->commandListCreate(context, d->deviceHdl, 0, &command_list, ZE_COMMAND_LIST_FLAG_EXPLICIT_ONLY);
	if (ret != ZE_RESULT_SUCCESS) {
		d->dev->memoryFree(context, device_input_value);
		d->dev->memoryFree(context, device_output_buffer);
		d->dev->moduleDestroy(module_handle);
		return ret;
	}

	ze_command_queue_handle_t command_queue;
	ret = d->dev->commandQueueCreate(context, d->deviceHdl, 0, 0, &command_queue, ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY);
	if (ret != ZE_RESULT_SUCCESS) {
		d->dev->memoryFree(context, device_input_value);
		d->dev->memoryFree(context, device_output_buffer);
		d->dev->moduleDestroy(module_handle);
		return ret;
	}

	ret = zeCommandListAppendMemoryCopy(command_list, device_output_buffer, &input_value, sizeof(float), nullptr, 0,
										nullptr);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't append memory copy: %s\n", l0_error_to_string(ret));
		d->dev->memoryFree(context, device_input_value);
		d->dev->memoryFree(context, device_output_buffer);
		d->dev->moduleDestroy(module_handle);
		return ret;
	}

	ret = zeCommandListAppendBarrier(command_list, nullptr, 0, nullptr);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't append barrier: %s\n", l0_error_to_string(ret));
		d->dev->memoryFree(context, device_input_value);
		d->dev->memoryFree(context, device_output_buffer);
		d->dev->moduleDestroy(module_handle);
		return ret;
	}

	ret = zeCommandListClose(command_list);
	if (ret != ZE_RESULT_SUCCESS) {
		ERR("Couldn't close command list: %s\n", l0_error_to_string(ret));
		d->dev->memoryFree(context, device_input_value);
		d->dev->memoryFree(context, device_output_buffer);
		d->dev->moduleDestroy(module_handle);
		return ret;
	}

	d->dev->commandQueueExecuteCommandLists(command_queue, command_list);
	d->dev->commandQueueSynchronize(command_queue);
	d->dev->commandListReset(command_list);
	ze_kernel_handle_t compute_sp_v1;
	d->dev->setupFunction(module_handle, compute_sp_v1, "compute_sp_v1", device_input_value, device_output_buffer);

	timed = 0;
	// Vector width 1
	timed = d->dev->runKernel(command_queue, command_list, compute_sp_v1, workgroup_info, checkOnly);
	out_gflops = calculateGbps(timed, (long double)number_of_work_items * flops_per_work_item);
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

	d->dev->memoryFree(context, device_input_value);
	d->dev->memoryFree(context, device_output_buffer);
	d->dev->moduleDestroy(module_handle);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Gets available tests within a diagnostic test suite
 *
 * This function retrieves all individual diagnostic tests available within
 * a specific test suite, providing detailed information about test indices
 * and names for comprehensive hardware validation capabilities.
 *
 * @param testSuite Handle to the specific diagnostic test suite
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
 * @param testSuite Handle to the specific diagnostic test suite
 * @param testCount Number of tests to execute within the suite
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
 * @param device Handle to the device for diagnostic operations
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