/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file intel_dnp_redfish_amc_manager.h
 */
#pragma once

#include "amc/redfish_amc_manager.h"

namespace xpum {

struct DNPRedfishHostInterface {
    std::string ipv4_addr;
    std::string ipv4_service_addr;
    std::string ipv4_service_mask;
    std::string interface_name;
    std::string idVendor;
    std::string idProduct;

    void genHostIp();

    bool valid() {
        // ipv4_service_port can be empty
        return ipv4_addr.length() &&
               ipv4_service_mask.length() &&
               interface_name.length() &&
               ipv4_service_addr.length() &&
               idVendor.length() &&
               idProduct.length();
    }
};

class DenaliPassRedfishAmcManager : public RedfishAmcManager {
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

    std::string initErrMsg;

    bool initialized = false;

    DNPRedfishHostInterface hostInterface;

    std::mutex mtx;

    std::vector<std::string> taskUriList;

    std::future<xpum_firmware_flash_result_t> task;

    std::string flashFwErrMsg;

    bool redfishHostInterfaceInit();

    bool bindIpToInterface();

    std::string interfaceName = "";
};
} // namespace xpum