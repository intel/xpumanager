/*
 *  Copyright (C) 2022-2023 Intel Corporation
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

    bool isPathExist(const std::string &s);

    void readConfigFile();

    double calculateMean(const std::vector<double>& data);

    double calcaulateVariance(const std::vector<double>& data);

    std::string zeResultErrorCodeStr(ze_result_t ret);

    void memoryAlloc(const ze_context_handle_t &context, ze_device_handle_t ze_device, size_t size, void **ptr);

    void memoryAllocHost(const ze_context_handle_t &context, size_t size, void **ptr);

    void memoryFree(const ze_context_handle_t &context, const void *ptr);

    void commandListCreate(const ze_context_handle_t &context, ze_device_handle_t ze_device, uint32_t command_queue_group_ordinal, ze_command_list_handle_t *phCommandList);

    void commandQueueCreate(const ze_context_handle_t &context, ze_device_handle_t ze_device, const uint32_t command_queue_group_ordinal, const uint32_t command_queue_index, ze_command_queue_handle_t *command_queue);

    bool zeDeviceCanAccessAllPeer(std::vector<ze_device_handle_t> ze_devices);

} // namespace xpum