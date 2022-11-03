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
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceIds, "The device ID or PCI BDF address");
    deviceIdOpt->delimiter(',');
#ifndef DAEMONLESS
    auto groupIdOpt = addOption("-g,--group", this->opts->groupId, "The group ID");
#endif
    deviceIdOpt->check([this](const std::string &str) {
#ifndef DAEMONLESS
    if (isGroupOperation())
        return std::string();
#endif
    std::string errStr = "Device id should be a non-negative integer or a BDF string";
    if (isValidDeviceId(str) || str == "-1") {
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
    
    auto stressFlag = addFlag("-s,--stress", this->opts->stress, "Stress the GPU(s) for the specified time");
    auto stressTimeOpt = addOption("--stresstime", this->opts->stressTime, "Stress time (in minutes)");
    auto preCheckOpt = addFlag("--precheck", this->opts->preCheck, "Do the precheck on the GPU and GPU driver");

    preCheckOpt->excludes(deviceIdOpt);
    preCheckOpt->excludes(level);
    preCheckOpt->excludes(stressFlag);
    preCheckOpt->excludes(stressTimeOpt);
    level->excludes(preCheckOpt);
    level->excludes(stressFlag);
    level->excludes(stressTimeOpt);
    deviceIdOpt->excludes(preCheckOpt);
    if (stressFlag == nullptr) {
        deviceIdOpt->needs(level);
    }
    stressTimeOpt->needs(stressFlag);

#ifndef DAEMONLESS
    preCheckOpt->excludes(groupIdOpt);
    groupIdOpt->excludes(preCheckOpt);
    groupIdOpt->excludes(stressFlag);
    groupIdOpt->excludes(stressTimeOpt);
    groupIdOpt->needs(level);
    stressFlag->needs(stressTimeOpt);
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
        if (this->opts->deviceIds[0] != "-1") {
            int targetId = -1;
            if (isNumber(this->opts->deviceIds[0])) {
                targetId = std::stoi(this->opts->deviceIds[0]);
            } else {
                auto convertResult = this->coreStub->getDeivceIdByBDF(this->opts->deviceIds[0].c_str(), &targetId);
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

    if (this->opts->stress) {
        if (isDeviceOperation()) {
            for (auto deviceId : this->opts->deviceIds) {
                json = this->coreStub->getDeviceProperties(std::stoi(deviceId));
                if (json->contains("error")) {
                    return json;
                }
            }
            for (auto deviceId : this->opts->deviceIds) {
                json = this->coreStub->runStress(std::stoi(deviceId), this->opts->stressTime);
                if (json->contains("error")) {
                    break;
                }
            }
            return json;
        } else {
            return this->coreStub->runStress(std::stoi(this->opts->deviceIds[0]), this->opts->stressTime);
        }
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

static void showStreesedDevices(std::ostream &out, const std::vector<std::string> &ids) {
    if (ids.size() <= 0) {
        return;
    }
    if (ids[0] == "-1") {
        out << "Stress on all GPU" << std::endl;;
        return;
    } else {
        std::string gpus;
        for (auto id : ids) {
            gpus += id + ",";
        }
        gpus.pop_back();
        out << "Stress on GPU " << gpus << std::endl;
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

    if (this->opts->stress) {
        showStreesedDevices(out, this->opts->deviceIds);
        auto begin = std::chrono::system_clock::now();
        while (true) {
            size_t fin_dev_count = 0;
            for (auto deviceId : this->opts->deviceIds) {
                json = this->coreStub->checkStress(std::stoi(deviceId));
                if (json->contains("error")) {
                    out << "Error: " << (*json)["error"].get<std::string>() << std::endl;
                    setExitCodeByJson(*json);
                    return;
                }
                auto tasks = (*json)["task_list"];
                size_t fin_task_count = 0;
                for (auto task : tasks) {
                    auto now = std::chrono::system_clock::now();
                    int pastSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - begin).count();
                    bool finished = task["finished"];
                    out << "Device: " << task["device_id"] << " Finished:" << finished << " Time: " << pastSeconds << " seconds" << std::endl;
                    if (finished) {
                        fin_task_count++;
                    }
                }
                if (tasks.size() == fin_task_count) {
                    fin_dev_count++;
                }
            }
            if (fin_dev_count == this->opts->deviceIds.size()) {
                out << "Finish stressing." << std::endl;
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        return;
    }

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
