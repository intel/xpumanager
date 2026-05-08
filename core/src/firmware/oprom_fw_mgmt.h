/*
 *  Copyright (C) 2021-2025 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file latebinding_mgmt.h
 */

#pragma once

#include <atomic>
#include <future>
#include <mutex>
#include <string>

#include "device/device.h"

#define MAX_CONNECT_RETRIES 3

namespace xpum {

struct FlashOpromFwParam {
    std::string filePath;
    xpum_firmware_type_t type;
    std::string errMsg;
};

struct GetFlashOpromFwResultParam {
    std::string errMsg;
};

class OpromFwMgmt {
   public:
    OpromFwMgmt(std::string devicePath, std::shared_ptr<Device> pDevice) : devicePath(devicePath), pDevice(pDevice){};

    xpum_result_t flashOpromFw(FlashOpromFwParam &param);

    xpum_firmware_flash_result_t getFlashOpromFwResult(GetFlashOpromFwResultParam &param);

    std::atomic<int> percent;

   private:
    std::string devicePath;

    std::mutex mtx; // lock for task
    std::future<xpum_firmware_flash_result_t> task;

    std::shared_ptr<Device> pDevice;

    std::string flashFwErrMsg;
};

} // namespace xpum
