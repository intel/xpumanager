/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file redfish_amc_manager.h
 */
#pragma once

#include <mutex>
#include <future>

#include "amc_manager.h"

namespace xpum {

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

class RedfishAmcManager : public AmcManager {
   public:
    virtual bool preInit() override;
    virtual bool init(InitParam& param) override;
    virtual std::string getProtocol() override {
        return "redfish";
    }
    virtual void flashAMCFirmware(FlashAmcFirmwareParam& param) override;

    virtual void getAmcFirmwareVersions(GetAmcFirmwareVersionsParam& param) override;

    virtual void getAMCFirmwareFlashResult(GetAmcFirmwareFlashResultParam& param) override;

    virtual void getAMCSensorReading(GetAmcSensorReadingParam& param) override;

    virtual void getAMCSlotSerialNumbers(GetAmcSlotSerialNumbersParam& param) override;

   private:
    bool initialized = false;

    RedfishHostInterface hostInterface;

    std::mutex mtx;

    std::vector<std::string> taskUriList;

    std::future<xpum_firmware_flash_result_t> task;

    std::string flashFwErrMsg;

    bool redfishHostInterfaceInit();

    bool bindIpToInterface();

};

RedfishHostInterface parseInterface(std::string dmiDecodeOutput);

std::string getRedfishAmcWarn();

} // namespace xpum