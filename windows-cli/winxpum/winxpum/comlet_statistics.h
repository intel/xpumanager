/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_statistics.h
 */

#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"

struct ComletStatisticsOptions {
    int deviceId = -1;
    uint32_t groupId = 0;
};

class ComletStatistics : public ComletBase {
public:
    ComletStatistics() : ComletBase("stats", "List the GPU aggregated statistics.") {
        printHelpWhenNoArgs = true;
    }
    virtual ~ComletStatistics() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

    virtual void getTableResult(std::ostream& out) override;

    inline const bool isDeviceOp() const {
        return opts->deviceId >= 0;
    }

    inline const bool isGroupOp() const {
        return opts->groupId != 0;
    }

    inline const int getDeviceId() const {
        return opts->deviceId;
    }

private:
    std::unique_ptr<ComletStatisticsOptions> opts;
};
