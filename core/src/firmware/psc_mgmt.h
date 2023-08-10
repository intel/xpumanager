/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file psc_mgmt.h
 */

#pragma once

#include <atomic>
#include <future>
#include <mutex>
#include <string>

#include "device/device.h"

namespace xpum {

struct FlashPscFwParam {
    std::string filePath;
    bool force;
    std::string errMsg;
};

struct GetFlashPscFwResultParam{
    std::string errMsg;
};

class PscMgmt {
   public:
    PscMgmt(std::string devicePath, std::shared_ptr<Device> pDevice) : devicePath(devicePath), pDevice(pDevice){};

    xpum_result_t flashPscFw(FlashPscFwParam &param);

    xpum_firmware_flash_result_t getFlashPscFwResult(GetFlashPscFwResultParam &param);

    void getPscFwVersion();
    std::atomic<int> percent;

   private:
    std::string devicePath;

    std::mutex mtx; // lock for task
    std::future<xpum_firmware_flash_result_t> task;

    std::shared_ptr<Device> pDevice;

    std::string flashFwErrMsg;
};

} // namespace xpum