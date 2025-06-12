/*
 *  Copyright (C) 2021-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file smc_redfish_amc_manager.h
 */
#pragma once

#include "amc/redfish_amc_manager.h"

namespace xpum {

enum SMCServerModel {
    SMC_2U_SYS_620C_TN12R_RSC_D2_668G4,
    SMC_2U_SYS_620C_TN12R_RSC_D2R_668G4,
    SMC_4U_SYS_420GP_TNR,
    SMC_SYS_821GV_TNR,
    SMC_UNKNOWN
};

struct RedfishHostInterface {
    std::string ipv4_addr;
    std::string ipv4_mask;
    std::string ipv4_service_addr;
    std::string ipv4_service_port;
    std::string interface_name;
    std::string idVendor;
    std::string idProduct;

    bool valid() {
        // ipv4_service_port can be empty
        return ipv4_addr.length() &&
               ipv4_mask.length() &&
               interface_name.length() &&
               ipv4_service_addr.length() &&
               idVendor.length() &&
               idProduct.length();
    }
};

class SMCRedfishAmcManager : public RedfishAmcManager {
   public:
    virtual bool preInit() override;
    virtual bool init(InitParam& param) override;
    virtual void flashAMCFirmware(FlashAmcFirmwareParam& param) override;

    virtual void getAmcFirmwareVersions(GetAmcFirmwareVersionsParam& param) override;

    virtual void getAMCFirmwareFlashResult(GetAmcFirmwareFlashResultParam& param) override;

    virtual void getAMCSensorReading(GetAmcSensorReadingParam& param) override;

    virtual void getAMCSlotSerialNumbers(GetAmcSlotSerialNumbersParam& param) override;

    static std::string getRedfishAmcWarn();

   private:

    SMCServerModel _model;

    bool initialized = false;

    RedfishHostInterface hostInterface;

    std::mutex mtx;

    std::vector<std::string> taskUriList;

    std::future<xpum_firmware_flash_result_t> task;

    std::string flashFwErrMsg;

    bool redfishHostInterfaceInit();

    bool bindIpToInterface();
};
} // namespace xpum