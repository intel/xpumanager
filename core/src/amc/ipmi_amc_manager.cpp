/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file ipmi_amc_manager.cpp
 */

#include "ipmi_amc_manager.h"
#include "infrastructure/logger.h"

#include <vector>
#include <string>
#include <sstream>
#include "ipmi/ipmi.h"

namespace xpum {

static void percent_callback(uint32_t percent, void* pAmcManager) {
    IpmiAmcManager* p = (IpmiAmcManager*)pAmcManager;
    if (p->percent.load() < (int)percent)
        p->percent.store(percent);
}

bool IpmiAmcManager::preInit(){
    XPUM_LOG_INFO("IpmiAmcManager preInit");
    InitParam param;
    return init(param);
}

static std::string initErrMsg;

bool IpmiAmcManager::init(InitParam& param) {
    if (initialized){
        param.errMsg = initErrMsg;
        return initSuccess;
    }
    updateAmcFwList();
    initialized = true;
    if (amcFwList.size() == 0) {
        initErrMsg = "Can not find AMC device through ipmi";
        param.errMsg = initErrMsg;
        XPUM_LOG_INFO("IpmiAmcManager can not find AMC device");
        initSuccess = false;
        return false;
    }
    XPUM_LOG_INFO("IpmiAmcManager init");
    initSuccess = true;
    return true;
}

static std::vector<std::string> getAMCFwVersionsInternal() {
    std::vector<std::string> versions;
    int count;
    int err = cmd_get_amc_firmware_versions(nullptr, &count);
    if (err != 0 || count <= 0) {
        return versions;
    }
    int buf[count][4];
    err = cmd_get_amc_firmware_versions(buf, &count);
    if (err != 0) {
        return versions;
    }
    for (int i = 0; i < count; i++) {
        std::stringstream ss;
        ss << buf[i][0] << ".";
        ss << buf[i][1] << ".";
        ss << buf[i][2] << ".";
        ss << buf[i][3];
        versions.push_back(ss.str());
    }
    return versions;
}

void IpmiAmcManager::updateAmcFwList() {
    amcFwList = getAMCFwVersionsInternal();
}

void IpmiAmcManager::getAmcFirmwareVersions(GetAmcFirmwareVersionsParam& param) {
    if (fwUpdated) {
        updateAmcFwList();
        fwUpdated = false;
    }
    for (auto version : amcFwList) {
        param.versions.push_back(version);
    }
    param.errCode = xpum_result_t::XPUM_OK;
}

void IpmiAmcManager::flashAMCFirmware(FlashAmcFirmwareParam& param) {
    std::lock_guard<std::mutex> lck(mtx);
    if (task.valid()) {
        param.errCode =  xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
        param.callback();
        return;
    }
    percent.store(0);
    flashFwErrMsg.clear();
    task = std::async(std::launch::async, [param, this] {

        fwUpdated = true;

        setPercentCallbackAndContext(percent_callback, this);

        int rc = cmd_firmware(param.file.c_str(), nullptr);

        auto result = rc == 0 ? xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK : xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;

        flashFwErrMsg = xpum::getIpmiErrorString(rc);

        param.callback();

        return result;
    });

    param.errCode = xpum_result_t::XPUM_OK;
}


void IpmiAmcManager::getAMCFirmwareFlashResult(GetAmcFirmwareFlashResultParam& param) {
    std::future<xpum_firmware_flash_result_t>* p_task = &task;

    xpum_firmware_flash_result_t res;

    if (p_task->valid()) {
        using namespace std::chrono_literals;
        auto status = p_task->wait_for(0ms);
        if (status == std::future_status::ready) {
            std::lock_guard<std::mutex> lck(mtx);
            res = p_task->get();
        } else {
            res = xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ONGOING;
        }
    } else {
        res = xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
    }
    auto& result = param.result;
    result.deviceId = XPUM_DEVICE_ID_ALL_DEVICES;
    result.type = XPUM_DEVICE_FIRMWARE_AMC;
    result.result = res;
    result.percentage = percent.load();
    param.errCode = XPUM_OK;
    param.errMsg = flashFwErrMsg;
}

void IpmiAmcManager::getAMCSensorReading(GetAmcSensorReadingParam& param){
    auto readingDataList = read_sensor();
    param.dataList = readingDataList;
    param.errCode = XPUM_OK;
}

void IpmiAmcManager::getAMCSlotSerialNumbers(GetAmcSlotSerialNumbersParam& param) {
}

void IpmiAmcManager::getAMCSerialNumberByRiserSlot(uint8_t baseboardSlot, uint8_t riserSlot, std::string &serialNumber) {
    if (int err = get_sn_number(baseboardSlot, riserSlot, serialNumber)) {
        XPUM_LOG_ERROR("Get AMC Serial Number failed, NRV error code: {}", err);
    }
}

} // namespace xpum