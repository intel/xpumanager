/*
 *  Copyright (C) 2021-2022 Intel Corporation
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

class PscMgmt {
   public:
    PscMgmt(std::string devicePath, std::shared_ptr<Device> pDevice) : devicePath(devicePath), pDevice(pDevice){};

    xpum_result_t flashPscFw(std::string filePath);

    xpum_firmware_flash_result_t getFlashPscFwResult();

    void getPscFwVersion();
    std::atomic<int> percent;

   private:
    std::string devicePath;

    std::mutex mtx; // lock for task
    std::future<xpum_firmware_flash_result_t> task;

    std::shared_ptr<Device> pDevice;
};

} // namespace xpum