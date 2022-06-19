/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file ipmi_amc_manager.h
 */
#pragma once

#include<future>

#include "amc_manager.h"

namespace xpum {
class IpmiAmcManager : public AmcManager {
   private:
    std::mutex mtx;

    std::future<xpum_firmware_flash_result_t> task;

   public:
    virtual bool init() override;
    virtual std::string getProtocol() override {
        return "ipmi";
    }
    virtual void flashAMCFirmware(FlashAmcFirmwareParam& param) override;
    virtual void getAmcFirmwareVersions(GetAmcFirmwareVersionsParam& param) override;
    virtual void getAMCFirmwareFlashResult(GetAmcFirmwareFlashResultParam& param) override;
};
} // namespace xpum