/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file amc_features.h
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include "../xpum_structs.h"

using namespace std;
namespace xpum {

struct InitParam {
    std::string errMsg;
};

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
/*
struct GetAmcSensorReadingParam{
    std::vector<xpum_sensor_reading_t> dataList;
    xpum_result_t errCode;
    std::string errMsg;
};
*/
struct SlotSerialNumberAndFwVersion{
    uint8_t baseboardSlot;
    uint8_t riserSlot;
    int slotId;
    std::string serialNumber;
    std::string firmwareVersion;
};

struct GetAmcSlotSerialNumbersParam{
    std::string username;
    std::string password;
    std::string errMsg;
    std::vector<SlotSerialNumberAndFwVersion> serialNumberList;
};

class AmcManager {
   public:
    std::atomic<int> percent;
    virtual bool preInit() = 0;
    virtual bool init(InitParam& param) = 0;
    virtual std::string getProtocol() = 0;
    virtual void flashAMCFirmware(FlashAmcFirmwareParam& param) = 0;
    virtual void getAmcFirmwareVersions(GetAmcFirmwareVersionsParam& param) = 0;
    virtual void getAMCFirmwareFlashResult(GetAmcFirmwareFlashResultParam& param) = 0;
    //virtual void getAMCSensorReading(GetAmcSensorReadingParam& param) = 0;
    virtual void getAMCSlotSerialNumbers(GetAmcSlotSerialNumbersParam& param) = 0;
};
} // namespace xpum
