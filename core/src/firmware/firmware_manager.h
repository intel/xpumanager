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
    xpum_result_t runAMCFirmwareFlash(const char* filePath);
    xpum_firmware_flash_result_t getAMCFirmwareFlashResult();

};
} // namespace xpum