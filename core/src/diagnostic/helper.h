/*
 *  Copyright (C) 2022-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file helper.h
 */
#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include "infrastructure/exception/base_exception.h"
#include "infrastructure/handle_lock.h"
#include "level_zero/ze_api.h"
#include "level_zero/zes_api.h"
#include "level_zero/zet_api.h"

namespace xpum {

    const std::string DIAG_CONFIG_THRESHOLD_CONIG_FILE = "diagnostics.conf";

    const std::string XPUM_GLOBAL_CONFIG_FILE = "xpum.conf";

    bool isPathExist(const std::string &s);

    void readConfigFile(std::string conf_file_name);

    double calculateMean(const std::vector<double>& data);

    double calcaulateVariance(const std::vector<double>& data);

    std::string zeResultErrorCodeStr(ze_result_t ret);

    void contextCreate(ze_driver_handle_t hDriver, ze_context_handle_t *hContext);

    void contextDestroy(ze_context_handle_t hContext);

    void moduleCreate(const ze_context_handle_t &context, ze_device_handle_t ze_device, std::vector<uint8_t> binary_file, ze_module_handle_t *module_handle);

    void moduleDestroy(ze_module_handle_t hModule);

    void kernelCreate(ze_module_handle_t hModule, std::string name, ze_kernel_handle_t *hKernel);

    void kernelDestroy(ze_kernel_handle_t hKernel);

    void kernelSetGroupSize(ze_kernel_handle_t hKernel, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ);

    void kernelSetArgumentValue(ze_kernel_handle_t hKernel, uint32_t argIndex, size_t argSize, const void *pArgValue);

    void memoryAlloc(const ze_context_handle_t &context, ze_device_handle_t ze_device, size_t size, size_t alignment, void **ptr);

    void memoryAllocShared(const ze_context_handle_t &context, ze_device_handle_t ze_device, size_t size, size_t alignment, void **ptr);

    void memoryAllocHost(const ze_context_handle_t &context, size_t size, size_t alignment, void **ptr);

    void memoryFree(const ze_context_handle_t &context, const void *ptr);

    void commandQueueCreate(const ze_context_handle_t &context, ze_device_handle_t ze_device, const uint32_t command_queue_group_ordinal, const uint32_t command_queue_index, ze_command_queue_handle_t *phCommandQueue, uint32_t flags = 0);

    void commandQueueExecuteCommandLists(ze_command_queue_handle_t hCommandQueue, ze_command_list_handle_t hCommandList);

    void commandQueueSynchronize(ze_command_queue_handle_t hCommandQueue);

    void commandQueueDestroy(ze_command_queue_handle_t hCommandQueue);

    void commandListCreate(const ze_context_handle_t &context, ze_device_handle_t ze_device, uint32_t command_queue_group_ordinal, ze_command_list_handle_t *phCommandList, uint32_t flags = 0);

    void commandListAppendBarrier(ze_command_list_handle_t hCommandList);

    void commandListAppendLaunchKernel(ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel, const ze_group_count_t *pLaunchFuncArgs);

    void commandListAppendMemoryCopy(ze_command_list_handle_t hCommandList, void *dstptr, const void *srcptr, size_t size);

    void commandListAppendMemoryFill(ze_command_list_handle_t hCommandList, void *ptr, const void *pattern, size_t pattern_size, size_t size);

    void commandListReset(ze_command_list_handle_t hCommandList);

    void commandListClose(ze_command_list_handle_t hCommandList);

    void commandListDestroy(ze_command_list_handle_t hCommandList);

    bool zeDeviceCanAccessAllPeer(std::vector<ze_device_handle_t> ze_devices);

} // namespace xpum