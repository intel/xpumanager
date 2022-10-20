/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_health.h
 */

#pragma once

#include <climits>
#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletHealthOptions {
    bool listAll = false;
    std::string deviceId = "-1";
    uint32_t groupId = UINT_MAX;
    int componentType = INT_MIN;
    int threshold = INT_MIN;
};

class ComletHealth : public ComletBase {
   public:
    ComletHealth() : ComletBase("health", "Get the GPU device component health status.") {
        printHelpWhenNoArgs = true;
    }
    virtual ~ComletHealth() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

    virtual void getTableResult(std::ostream &out) override;

    inline const bool isSingleDeviceOperation() const {
        return opts->deviceId != "-1";
    }

    inline const bool isGroupOperation() const {
        return opts->groupId > 0;
    }

    inline const bool isListAll() const {
        return opts->listAll;
    }

    inline const int getCompType() const {
        return opts->componentType;
    }

   private:
    std::unique_ptr<ComletHealthOptions> opts;
};
} // end namespace xpum::cli
