/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_diagnostic.cpp
 */

#include "comlet_diagnostic.h"

#include <nlohmann/json.hpp>

#include "cli_table.h"
#include "core_stub.h"
#include "utility.h"
#include "exit_code.h"

namespace xpum::cli {

static CharTableConfig ComletConfigDiagnosticDevice(R"({
    "showTitleRow": false,
    "columns": [{
        "title": "none"
    }, {
        "title": "none"
    }],
    "rows": [{
        "instance": "",
        "cells": [
            { "rowTitle": "Device ID" },
            "device_id"
        ]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "Level" },
            { "rowTitle": "Result" },
            { "rowTitle": "Items" }
        ], [
            { "value": "level" },
            { "value": "result" },
            { "value": "component_count" }
        ]]
    }, {
        "instance": "component_list[]",
        "cells": [
            { "value": "component_type" }, [
            { "label": "Result", "value": "result" },
            { "label": "Message", "value": "message" },
            { "value": "process_list[]", "subrow": true, "subs": [
                { "label": "  PID", "value": "process_id" },
                { "label": "Command", "value": "process_name" }
            ]},
            { "value": "media_codec_list[]", "subrow": true, "subs": [
                { "label": "", "value": "fps" }
            ]}
        ]]
    }]
})"_json);

static CharTableConfig ComletConfigDiagnosticPreCheck(R"({
    "showTitleRow": true,
    "columns": [{
        "title": "Component",
        "size": 16
    }, {
        "title": "Status"
    }],
    "rows": [{
        "instance": "",
        "cells": [[
            { "rowTitle": "GPU" },
            { "rowTitle": "Driver" },
            { "rowTitle": "GPU Status" },
            { "rowTitle": "CPU Status" }
        ], [
            { "value": "gpu_basic_info" },
            { "value": "gpu_driver_info" },
            { "value": "gpu_status_info" },
            { "value": "cpu_status_info" }
        ]]
    }]
})"_json);

void ComletDiagnostic::setupOptions() {
    this->opts = std::unique_ptr<ComletDiagnosticOptions>(new ComletDiagnosticOptions());
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceId, "The device ID or PCI BDF address");
#ifndef DAEMONLESS
    auto groupIdOpt = addOption("-g,--group", this->opts->groupId, "The group ID");
#endif
    deviceIdOpt->check([this](const std::string &str) {
#ifndef DAEMONLESS
    if (isGroupOperation())
        return std::string();
#endif
    std::string errStr = "Device id should be a non-negative integer or a BDF string";
    if (isValidDeviceId(str)) {
        return std::string();        
    } else if (isBDF(str)) {
        return std::string();
    }
    return errStr;
    });
    auto level = addOption("-l,--level", this->opts->level,
              "The diagnostic levels to run. The valid options include\n\
      1. quick test\n\
      2. medium test - this diagnostic level will have the significant performance impact on the specified GPUs\n\
      3. long test - this diagnostic level will have the significant performance impact on the specified GPUs");
    auto preCheckOpt = addFlag("--precheck", this->opts->preCheck, "Do the precheck on the GPU and GPU driver");

    preCheckOpt->excludes(deviceIdOpt);
    preCheckOpt->excludes(level);
    level->excludes(preCheckOpt);
    deviceIdOpt->excludes(preCheckOpt);
    deviceIdOpt->needs(level);

#ifndef DAEMONLESS
    preCheckOpt->excludes(groupIdOpt);
    groupIdOpt->excludes(preCheckOpt);
    groupIdOpt->needs(level);
#endif
}

std::unique_ptr<nlohmann::json> ComletDiagnostic::run() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
#ifndef DAEMONLESS
    if (this->opts->groupId == 0) {
        (*json)["error"] = "group not found";
        (*json)["errno"] = XPUM_CLI_ERROR_GROUP_NOT_FOUND;
        return json;
    }
#endif

    if (this->opts->level != INT_MIN && (this->opts->level < 1 || this->opts->level > 3)) {
        (*json)["error"] = "invalid level";
        (*json)["errno"] = XPUM_CLI_ERROR_DIAGNOSTIC_INVALID_LEVEL;
        return json;
    }
    if (this->opts->level >= 1 && this->opts->level <= 3) {
        if (this->opts->deviceId != "-1") {
            int targetId = -1;
            if (isNumber(this->opts->deviceId)) {
                targetId = std::stoi(this->opts->deviceId);
            } else {
                auto convertResult = this->coreStub->getDeivceIdByBDF(this->opts->deviceId.c_str(), &targetId);
                if (convertResult->contains("error")) {
                    return convertResult;
                }
            }
            json = this->coreStub->runDiagnostics(targetId, this->opts->level, this->opts->rawComponentTypeStr);
            return json;
        } 
#ifndef DAEMONLESS
        else if (this->opts->groupId > 0 && this->opts->groupId != UINT_MAX) {
            json = this->coreStub->runDiagnosticsByGroup(this->opts->groupId, this->opts->level, this->opts->rawComponentTypeStr);
            return json;
        }
#endif
    }

    if (this->opts->preCheck) {
        json = this->coreStub->getPreCheckInfo();
        return json;
    }
    (*json)["error"] = "Wrong argument or unknown operation, run with --help for more information.";
    (*json)["errno"] = XPUM_CLI_ERROR_BAD_ARGUMENT;
    return json;
}

static void showDeviceDiagnostic(std::ostream &out, std::shared_ptr<nlohmann::json> json, bool precheck = false, const bool cont = false) {
    if (precheck) {
        CharTable table(ComletConfigDiagnosticPreCheck, *json, cont);
        table.show(out);
    } else {
        CharTable table(ComletConfigDiagnosticDevice, *json, cont);
        table.show(out);
    }
}

void ComletDiagnostic::getTableResult(std::ostream &out) {
    this->opts->rawComponentTypeStr = false;
    auto res = run();
    this->opts->rawComponentTypeStr = true;
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        setExitCodeByJson(*res);
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;

#ifndef DAEMONLESS
    if (isGroupOperation()) {
        auto devices = (*json)["device_list"].get<std::vector<nlohmann::json>>();
        bool cont = false;
        for (auto device : devices) {
            showDeviceDiagnostic(out, std::make_shared<nlohmann::json>(device), false, cont);
            cont = true;
        }
        return;
    }
#endif
    if (isDeviceOperation()) {
        showDeviceDiagnostic(out, json);
        return;
    }

    if (this->opts->preCheck) {
        showDeviceDiagnostic(out, json, true);
        return;
    }
}
} // end namespace xpum::cli
