/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file redfish_amc_manager.h
 */
#pragma once

#include <future>
#include <mutex>

#include "amc_manager.h"

namespace xpum {

class RedfishAmcManager : public AmcManager {
   public:
    virtual std::string getProtocol() override {
        return "redfish";
    }

    static std::shared_ptr<AmcManager> instance();
};

std::string getRedfishAmcWarn();

} // namespace xpum