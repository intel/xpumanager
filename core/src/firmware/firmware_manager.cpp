/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file dump_manager.h
 */
#include "firmware_manager.h"

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace xpum {

extern int cmd_firmware(const char* file, unsigned int versions[4]);

extern std::vector<std::string> cmd_get_amc_firmware_versions();

void FirmwareManager::getAMCFwVersions() {
    if(amcUpdated){
        // firmware updated, need to re get version info
        amcFwList = cmd_get_amc_firmware_versions();
        amcUpdated = false;
    }
}

void FirmwareManager::init() {
    // get amc fw versions
    amcFwList = cmd_get_amc_firmware_versions();
};

std::vector<std::string> FirmwareManager::getAMCFirmwareVersions() {
    getAMCFwVersions();
    return amcFwList;
}

xpum_result_t FirmwareManager::runAMCFirmwareFlash(const char* filePath) {
    if (amcFwList.size() <= 0) {
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC;
    }

    std::lock_guard<std::mutex> lck(mtx);
    if (taskAMC.valid()) {
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
    } else {
        std::string dupPath(filePath);
        taskAMC = std::async(std::launch::async, [dupPath, this] {
            int rc = cmd_firmware(dupPath.c_str(), nullptr);
            this->amcUpdated = true;
            if (rc == 0) {
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
            } else {
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }
        });

        return xpum_result_t::XPUM_OK;
    }
}

xpum_firmware_flash_result_t FirmwareManager::getAMCFirmwareFlashResult() {
    std::future<xpum_firmware_flash_result_t>* task = &taskAMC;

    if (task->valid()) {
        using namespace std::chrono_literals;
        auto status = task->wait_for(0ms);
        if (status == std::future_status::ready) {
            std::lock_guard<std::mutex> lck(mtx);
            return task->get();
        } else {
            return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ONGOING;
        }
    } else {
        return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
    }
}

bool FirmwareManager::isUpgradingFw(void) {
    return taskAMC.valid();
}
} // namespace xpum