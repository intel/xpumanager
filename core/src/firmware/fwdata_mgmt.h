/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file fwdata_mgmt.h
 */

#pragma once

#include <future>
#include <mutex>
#include <string>

#include "device/device.h"

namespace xpum {

class FwDataMgmt {
   public:
    FwDataMgmt(std::string devicePath, std::shared_ptr<Device> pDevice) : devicePath(devicePath), pDevice(pDevice){};

    xpum_result_t flashFwData(std::string filePath);

    xpum_firmware_flash_result_t getFlashFwDataResult();

    void getFwDataVersion();

    bool isUpgradingFw();

    bool isReady();

   private:
    std::string devicePath;

    std::mutex mtx;
    std::future<xpum_firmware_flash_result_t> taskFwData;

    std::shared_ptr<Device> pDevice;
};

} // namespace xpum
