/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_diagnostic.h
 */

#pragma once

#include <climits>
#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletDiagnosticOptions {
    std::string deviceId = "-1";
#ifndef DAEMONLESS
    uint32_t groupId = UINT_MAX;
#endif
    int level = INT_MIN;
    bool rawComponentTypeStr = true;
    bool preCheck = false;
};

class ComletDiagnostic : public ComletBase {
   public:
    ComletDiagnostic() : ComletBase("diag", "Run some test suites to diagnose GPU.") {
        printHelpWhenNoArgs = true;
    }
    virtual ~ComletDiagnostic() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

    virtual void getTableResult(std::ostream &out) override;

    inline const bool isDeviceOperation() const {
        return opts->deviceId != "-1";
    }

#ifndef DAEMONLESS
    inline const bool isGroupOperation() const {
        return opts->groupId > 0 && opts->groupId != UINT_MAX;
    }
#endif

    inline const int getLevel() const {
        return opts->level;
    }

   private:
    std::unique_ptr<ComletDiagnosticOptions> opts;
};
} // end namespace xpum::cli
