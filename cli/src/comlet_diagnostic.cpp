/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_diagnostic.cpp
 */

#include "comlet_diagnostic.h"

#include <nlohmann/json.hpp>

#include "cli_table.h"
#include "core_stub.h"

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
            ]}
        ]]
    }]
})"_json);

void ComletDiagnostic::setupOptions() {
    this->opts = std::unique_ptr<ComletDiagnosticOptions>(new ComletDiagnosticOptions());
    addOption("-d,--device", this->opts->deviceId, "The device ID");
    addOption("-g,--group", this->opts->groupId, "The group ID");
    addOption("-l,--level", this->opts->level,
              "The diagnostic levels to run. The valid options include\n\
      1. quick test\n\
      2. medium test - this diagnostic level will have the significant performance impact on the specified GPUs\n\
      3. long test - this diagnostic level will have the significant performance impact on the specified GPUs");
}

std::unique_ptr<nlohmann::json> ComletDiagnostic::run() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    if (this->opts->deviceId != INT_MIN && this->opts->deviceId < 0) {
        (*json)["error"] = "device not found";
        return json;
    }

    if (this->opts->groupId == 0) {
        (*json)["error"] = "group not found";
        return json;
    }

    if (this->opts->level != INT_MIN && (this->opts->level < 1 || this->opts->level > 3)) {
        (*json)["error"] = "invalid level";
        return json;
    }
    if (this->opts->level >= 1 && this->opts->level <= 3) {
        if (this->opts->deviceId >= 0) {
            json = this->coreStub->runDiagnostics(this->opts->deviceId, this->opts->level, this->opts->rawComponentTypeStr);
            return json;
        } else if (this->opts->groupId > 0 && this->opts->groupId != UINT_MAX) {
            json = this->coreStub->runDiagnosticsByGroup(this->opts->groupId, this->opts->level, this->opts->rawComponentTypeStr);
            return json;
        }
    }
    (*json)["error"] = "Wrong argument or unknown operation, run with --help for more information.";
    return json;
}

static void showDeviceDiagnostic(std::ostream &out, std::shared_ptr<nlohmann::json> json, const bool cont = false) {
    CharTable table(ComletConfigDiagnosticDevice, *json, cont);
    table.show(out);
}

void ComletDiagnostic::getTableResult(std::ostream &out) {
    this->opts->rawComponentTypeStr = false;
    auto res = run();
    this->opts->rawComponentTypeStr = true;
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;

    if (isGroupOperation()) {
        auto devices = (*json)["device_list"].get<std::vector<nlohmann::json>>();
        bool cont = false;
        for (auto device : devices) {
            showDeviceDiagnostic(out, std::make_shared<nlohmann::json>(device), cont);
            cont = true;
        }
        return;
    }
    if (isDeviceOperation()) {
        showDeviceDiagnostic(out, json);
        return;
    }
}
} // end namespace xpum::cli
