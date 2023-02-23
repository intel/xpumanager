/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file hpe_redfish_amc_manager.h
 */
#pragma once

#include "amc/redfish_amc_manager.h"

namespace xpum {
class HEPRedfishAmcManager : public RedfishAmcManager {
   public:
    virtual bool preInit() override;
    virtual bool init(InitParam& param) override;
    
    virtual void getAmcFirmwareVersions(GetAmcFirmwareVersionsParam& param) override;

    virtual void flashAMCFirmware(FlashAmcFirmwareParam& param) override;

    virtual void getAMCFirmwareFlashResult(GetAmcFirmwareFlashResultParam& param) override;

    virtual void getAMCSensorReading(GetAmcSensorReadingParam& param) override;

    virtual void getAMCSlotSerialNumbers(GetAmcSlotSerialNumbersParam& param) override;

    static std::string getRedfishAmcWarn();

   private:
    bool initialized = false;

    std::mutex mtx;

    std::vector<std::string> taskUriList;

    std::future<xpum_firmware_flash_result_t> task;

    std::string flashFwErrMsg;

    bool redfishHostInterfaceInit();

    bool activeInterfaceAndConfigDHCP();

    std::string interfaceName = "";
};
} // namespace xpum