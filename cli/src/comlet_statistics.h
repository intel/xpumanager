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
    uint32_t groupId = 0;
};

class ComletStatistics : public ComletBase {
   public:
#ifndef DAEMONLESS
    ComletStatistics() : ComletBase("stats", "List the GPU aggregated statistics since last execution of this command or XPU Manager daemon is started.") {
        printHelpWhenNoArgs = true;
    }
#else
    ComletStatistics() : ComletBase("stats", "List the GPU statistics.") {
        printHelpWhenNoArgs = true;
    }
#endif
    virtual ~ComletStatistics() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

    virtual void getTableResult(std::ostream &out) override;

    inline const bool isDeviceOp() const {
        return opts->deviceId != "-1";
    }

    inline const bool isGroupOp() const {
        return opts->groupId != 0;
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
