/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file ipmi_amc_manager.h
 */
#pragma once

#include <future>
#include <string>

#include "amc_manager.h"

namespace xpum {
class IpmiAmcManager : public AmcManager {
   private:
    bool initialized = false;

    bool initSuccess = false;

    std::mutex mtx;

    std::vector<std::string> amcFwList;

    std::future<xpum_firmware_flash_result_t> task;

    void updateAmcFwList();

    bool fwUpdated = false;

   public:
    virtual bool preInit() override;
    virtual bool init() override;
    virtual std::string getProtocol() override {
        return "ipmi";
    }
    virtual void flashAMCFirmware(FlashAmcFirmwareParam& param) override;
    virtual void getAmcFirmwareVersions(GetAmcFirmwareVersionsParam& param) override;
    virtual void getAMCFirmwareFlashResult(GetAmcFirmwareFlashResultParam& param) override;
    virtual void getAMCSensorReading(GetAmcSensorReadingParam& param) override;
};
} // namespace xpum