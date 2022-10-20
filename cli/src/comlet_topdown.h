/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_occupancy.h
 */

#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletTopdownOptions {
    std::string deviceId = "-1";
    int deviceTileId = -1;
    int samplingInterval = -1;
};

class ComletTopdown : public ComletBase {
   public:
    ComletTopdown();
    virtual ~ComletTopdown() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

    virtual void getTableResult(std::ostream &out) override;

    inline const bool isDeviceOperation() const {
        return opts->deviceId != "-1";
    }

   private:
    std::unique_ptr<ComletTopdownOptions> opts;
};

} // end namespace xpum::cli
