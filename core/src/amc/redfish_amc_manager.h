/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file redfish_amc_manager.h
 */
#pragma once

#include <future>
#include <mutex>

#include "amc_manager.h"

namespace xpum {

extern int XPUM_CURL_TIMEOUT;
class RedfishAmcManager : public AmcManager {
   public:
    RedfishAmcManager() {
        readConfigFile();
    };
    virtual std::string getProtocol() override {
        return "redfish";
    }

    static std::shared_ptr<AmcManager> instance();

    void readConfigFile();
};

std::string getRedfishAmcWarn();

} // namespace xpum