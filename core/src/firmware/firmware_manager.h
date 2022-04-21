/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file dump_manager.h
 */

#pragma once

#include <future>
#include <mutex>
#include <string>
#include <vector>

#include "xpum_structs.h"

namespace xpum {
class FirmwareManager {
   private:
    bool amcUpdated = false;
    std::vector<std::string> amcFwList;

    std::mutex mtx;

    std::future<xpum_firmware_flash_result_t> taskAMC;

    void getAMCFwVersions();

   public:
    void init();
    bool isUpgradingFw(void);

    std::vector<std::string> getAMCFirmwareVersions();
    void detectGscFw();
    xpum_result_t runAMCFirmwareFlash(const char* filePath);
    void getAMCFirmwareFlashResult(xpum_firmware_flash_task_result_t *result);
    xpum_result_t runGSCFirmwareFlash(xpum_device_id_t deviceId, const char* filePath);
    void getGSCFirmwareFlashResult(xpum_device_id_t deviceId, xpum_firmware_flash_task_result_t* result);
};
} // namespace xpum