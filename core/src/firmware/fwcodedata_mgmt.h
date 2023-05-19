/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file fwcodedata_mgmt.h
 */

#pragma once

#include <future>
#include <mutex>
#include <string>
#include <atomic>
#include <dirent.h>
#include <unistd.h>

#include "device/device.h"

namespace xpum {

bool unpackAndGetImagePath(const char* filePath, const char* dirName, int eccState, std::string &codeImagePath, std::string &dataImagePath);

bool removeDir(const char* dirPath);

struct FlashFwCodeDataParam {
    xpum_device_id_t deviceId;
    std::string codeImagePath;
    std::string dataImagePath;
    std::string errMsg;
};

struct GetFlashFwCodeDataResultParam{
    std::string errMsg;
};

class FwCodeDataMgmt {
   public:
    FwCodeDataMgmt(std::string devicePath, std::shared_ptr<Device> pDevice) : devicePath(devicePath), pDevice(pDevice){};

    xpum_result_t flashFwCodeData(FlashFwCodeDataParam &param);

    xpum_firmware_flash_result_t getFlashFwCodeDataResult(GetFlashFwCodeDataResultParam &param);

    bool isUpgradingFw();

    bool isReady();

    std::atomic<int> percent;

    bool isNeedUpdateData= false;

    std::string tmpUnpackPath = "/tmp/tmp_fw_update_for_xpum";

   private:
    std::string devicePath;

    std::mutex mtx;
    std::future<xpum_firmware_flash_result_t> taskFwCodeData;

    std::shared_ptr<Device> pDevice;

    std::string flashFwErrMsg;
};
}