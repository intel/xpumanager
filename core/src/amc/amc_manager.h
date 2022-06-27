/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file amc_features.h
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include "../include/xpum_structs.h"

namespace xpum {

struct FlashAmcFirmwareParam {
    std::string file;
    std::string username;
    std::string password;
    xpum_result_t errCode;
    std::string errMsg;
    std::function<void()> callback;
};

struct GetAmcFirmwareVersionsParam{
    std::string username;
    std::string password;
    std::vector<std::string> versions;
    xpum_result_t errCode;
    std::string errMsg;
};

struct GetAmcFirmwareFlashResultParam {
    std::string username;
    std::string password;
    xpum_result_t errCode;
    std::string errMsg;
    xpum_firmware_flash_task_result_t result;
};

struct GetAmcSensorReadingParam{
    std::vector<xpum_sensor_reading_t> dataList;
    xpum_result_t errCode;
    std::string errMsg;
};

class AmcManager {
   public:
    virtual bool preInit() = 0;
    virtual bool init() = 0;
    virtual std::string getProtocol() = 0;
    virtual void flashAMCFirmware(FlashAmcFirmwareParam& param) = 0;
    virtual void getAmcFirmwareVersions(GetAmcFirmwareVersionsParam& param) = 0;
    virtual void getAMCFirmwareFlashResult(GetAmcFirmwareFlashResultParam& param) = 0;
    virtual void getAMCSensorReading(GetAmcSensorReadingParam& param) = 0;
};
} // namespace xpum
