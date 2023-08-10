/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_diagnostic.h
 */

#pragma once

#include <climits>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletDiagnosticOptions {
    std::vector<std::string> deviceIds = {"-1"};
#ifndef DAEMONLESS
    uint32_t groupId = UINT_MAX;
#endif
    int level = INT_MIN;
    std::vector<int> singleTestIdList;
    bool rawJson = true;
    bool preCheck = false;
    bool listErrorType = false;
    bool onlyGPU = false;
    bool scanDmesg = false;
    uint32_t stressTime = 0;
    bool stress = false;
    std::string sinceTime;
};

enum ShowMode {
    LEVEL_TEST,
    SINGLE_TEST,
    PRE_CHECK,
    PRE_CHECK_ERROR_TYPE
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
        return opts->deviceIds[0] != "-1";
    }

#ifndef DAEMONLESS
    inline const bool isGroupOperation() const {
        return opts->groupId > 0 && opts->groupId != UINT_MAX;
    }
#endif

    inline const int getLevel() const {
        return opts->level;
    }

    bool isPreCheck();

   private:
    std::unique_ptr<ComletDiagnosticOptions> opts;
};
} // end namespace xpum::cli
