/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_statistics.h
 */

#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletStatisticsOptions {
    std::string deviceId = "-1";
    bool showEuMetrics = false;
    bool showRASMetrics = false;
};

class ComletStatistics : public ComletBase {
   public:
    ComletStatistics() : ComletBase("stats", "List the GPU statistics.") {
        printHelpWhenNoArgs = true;
    }

    virtual ~ComletStatistics() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

    virtual void getTableResult(std::ostream &out) override;

    inline const bool isDeviceOp() const {
        return opts->deviceId != "-1";
    }

    inline const std::string getDeviceId() const {
        return opts->deviceId;
    }

    bool hasEUMetrics();
    bool hasRASMetrics();

   private:
    std::unique_ptr<ComletStatisticsOptions> opts;
};
} // end namespace xpum::cli
