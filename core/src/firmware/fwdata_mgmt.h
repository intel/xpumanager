/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file fwdata_mgmt.h
 */

#pragma once

#include <future>
#include <mutex>
#include <string>
#include <atomic>

#include "device/device.h"

namespace xpum {

struct FlashFwDataParam {
    std::string filePath;
    std::string errMsg;
};

struct GetFlashFwDataResultParam {
    std::string errMsg;
};

class FwDataMgmt {
   public:
    FwDataMgmt(std::string devicePath, std::shared_ptr<Device> pDevice) : devicePath(devicePath), pDevice(pDevice){};

    xpum_result_t flashFwData(FlashFwDataParam &param);

    xpum_firmware_flash_result_t getFlashFwDataResult(GetFlashFwDataResultParam &param);

    void getFwDataVersion();

    bool isUpgradingFw();

    bool isReady();

    std::atomic<int> percent;

   private:
    std::string devicePath;

    std::mutex mtx;
    std::future<xpum_firmware_flash_result_t> taskFwData;

    std::shared_ptr<Device> pDevice;

    std::string flashFwErrMsg;
};

xpum_result_t isFwDataImageAndDeviceCompatible(std::vector<char>& buffer, std::string devicePath);

} // namespace xpum
