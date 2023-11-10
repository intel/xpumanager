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

typedef struct {
    std::string result;
    std::string message;
} ComponentResultMessage;

struct CombinedDiagResult {
    std::string combined_device_ids;
    std::string combined_start_time;
    std::string combined_result;
    // component type => {result, message}
    std::map<std::string, ComponentResultMessage> combinedComponentTypeResultMessages = {
        {"XPUM_DIAG_SOFTWARE_ENV_VARIABLES", {"Pass", " to check environment variables."}},
        {"XPUM_DIAG_SOFTWARE_LIBRARY", {"Pass", " to check libraries."}},
        {"XPUM_DIAG_SOFTWARE_PERMISSION", {"Pass", " to check permission."}},
        {"XPUM_DIAG_SOFTWARE_EXCLUSIVE", {"Pass", " to check the software exclusive."}},
        {"XPUM_DIAG_LIGHT_COMPUTATION", {"Pass", " to check computation."}},
        {"XPUM_DIAG_LIGHT_CODEC", {"Pass", " to check Media transcode functionality."}},
        {"XPUM_DIAG_INTEGRATION_PCIE", {"Pass", " to check PCIe bandwidth."}},
        {"XPUM_DIAG_MEDIA_CODEC", {"Pass", " to check Media transcode performance."}},
        {"XPUM_DIAG_PERFORMANCE_COMPUTATION", {"Pass", " to check computation performance."}},
        {"XPUM_DIAG_PERFORMANCE_POWER", {"Pass", " to check stress power."}},
        {"XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION", {"Pass", " to check memory allocation."}},
        {"XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH", {"Pass", " to check memory bandwidth."}},
        {"XPUM_DIAG_MEMORY_ERROR", {"Pass", " to check memory error."}},
        {"XPUM_DIAG_XE_LINK_THROUGHPUT", {"Pass", " to check Xe Link throughput."}}
    };
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

    std::unique_ptr<nlohmann::json> combineDeviceData(CombinedDiagResult& cr, std::shared_ptr<nlohmann::json> one);
};
} // end namespace xpum::cli
