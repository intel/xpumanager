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
#ifndef _DIAGNOSTIC_H
#define _DIAGNOSTIC_H

#include "sysman.h"

// Forward declaration of devInfo struct
struct devInfo;

class LIBXPUM_API diagnostic : public sysman
{
private:
	uint32_t testSuiteCount;
	zes_diag_handle_t *testSuites;

public:
	diagnostic() : testSuiteCount(0), testSuites(nullptr) {}
	~diagnostic();
	ze_result_t enumDiag(zes_device_handle_t device);
	ze_result_t getProperties(zes_diag_handle_t testSuite);
	ze_result_t computationTest(devInfo *d, std::size_t flops_per_work_item, void *srcPtr, size_t srcSize,
								const std::string &fileName, const std::string &moduleName, long double &out_gflops,
								bool functionalCheck);
	ze_result_t dispatchKernelsForMemoryTest(devInfo *d, ze_module_handle_t module,
											 std::vector<uint8_t *> srcAllocations,
											 std::vector<uint8_t *> dstAllocations,
											 std::vector<std::vector<uint8_t>> &dataOut,
											 const std::vector<std::string> &testKernelNames, uint64_t numberOfDispatch,
											 uint64_t oneCaseAllocationCount, ze_context_handle_t context);
	long double calculateGbpsForWorkGroups(ze_command_queue_handle_t commandQueue, ze_command_list_handle_t commandList,
										   ze_kernel_handle_t &function1, ze_kernel_handle_t &function2,
										   uint64_t numItems, uint64_t factor,
										   ze_device_compute_properties_t &zeComputeProp,
										   struct ZeWorkGroups &workgroupInfo);

	ze_result_t calculatePowerConsumption(devInfo *d, int &totalPowerValue);
	ze_result_t pcieBandwidthTest(devInfo *d, long double &totalBandwidth, long double &totalLatency);
	ze_result_t peformanceMemoryAllocation(devInfo *d);
	ze_result_t memoryErrorTest(devInfo *d, int &errorCount);
	ze_result_t memoryBandwidthTest(devInfo *d, std::vector<long double> &totalGbps);
	int getTests(zes_diag_handle_t testSuite);
	ze_result_t kernelCreate(ze_module_handle_t hModule, std::string name, ze_kernel_handle_t *hKernel);
	ze_result_t loadBinaryFile(const std::string &filePath, std::vector<uint8_t> *binary_file);
	ze_result_t moduleCreate(const ze_context_handle_t &context, ze_device_handle_t ze_device,
							 std::vector<uint8_t> binary_file, ze_module_handle_t *module_handle);
	void moduleDestroy(ze_module_handle_t hModule);
	void freeResource(ze_context_handle_t context, void *inputBuf, void *outputBuf, ze_module_handle_t moduleHandle);
	uint64_t setWorkgroups(ze_device_compute_properties_t *device_compute_properties,
						   const uint64_t total_work_items_requested, struct ZeWorkGroups *workgroup_info);
	ze_result_t memoryAlloc(const ze_context_handle_t context, ze_device_handle_t ze_device, size_t size,
							size_t alignment, void **ptr);
	ze_result_t memoryAllocHost(const ze_context_handle_t context, size_t size, size_t alignment, void **ptr);
	ze_result_t memoryAllocShared(const ze_context_handle_t &context, ze_device_handle_t ze_device, size_t size,
								  size_t alignment, void **ptr);
	ze_result_t memoryFree(const ze_context_handle_t &context, const void *ptr);
	void commandListAppendLaunchKernel(ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel,
									   const ze_group_count_t *pLaunchFuncArgs);
	ze_result_t commandListCreate(const ze_context_handle_t context, ze_device_handle_t ze_device,
								  uint32_t command_queue_group_ordinal, ze_command_list_handle_t *phCommandList,
								  uint32_t flags);
	ze_result_t commandQueueCreate(const ze_context_handle_t context, ze_device_handle_t ze_device,
								   const uint32_t command_queue_group_ordinal, const uint32_t command_queue_index,
								   ze_command_queue_handle_t *phCommandQueue, uint32_t flags);

	ze_result_t commandQueueExecuteCommandLists(ze_command_queue_handle_t hCommandQueue,
												ze_command_list_handle_t hCommandList);
	ze_result_t commandQueueSynchronize(ze_command_queue_handle_t hCommandQueue);
	ze_result_t setupFunction(ze_module_handle_t module_handle, ze_kernel_handle_t &function, const std::string &name,
							  void *input, void *output);
	long double runKernel(ze_command_queue_handle_t command_queue, ze_command_list_handle_t command_list,
						  ze_kernel_handle_t &function, struct ZeWorkGroups &workgroup_info, bool checkOnly);
	ze_result_t commandListReset(ze_command_list_handle_t hCommandList);
	ze_result_t runTests(zes_diag_handle_t testSuite, uint32_t testCount);
	ze_result_t zesRun(zes_device_handle_t device);
};

#endif
