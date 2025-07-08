/* 
 *  Copyright (C) 2021-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_diagnostic.cpp
 */

#include "comlet_diagnostic.h"

#include <nlohmann/json.hpp>

#include "cli_table.h"
#include "core_stub.h"
#include "utility.h"
#include "exit_code.h"
#include <set>
#include <unordered_map>


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
            ]},
            { "value": "xe_link_throughput_list[]", "subrow": true, "subs": [
                { "label": "", "value": "xe_link_throughput" }
            ]}
        ]]
    }]
})"_json);

static CharTableConfig ComletConfigSpecificDiagnosticDevice(R"({
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
            ]},
            { "value": "xe_link_throughput_list[]", "subrow": true, "subs": [
                { "label": "", "value": "xe_link_throughput" }
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
        "title": "Details"
    }],
    "rows": [{
        "instance": "component_list[]",
        "cells": [
            { "value": "type" }, [
            { "value": "error_details[]", "subrow": true, "subs": [
                { "value": "field_value" }
            ]}
        ]]
    }]
})"_json);

static CharTableConfig ComletConfigDiagnosticPreCheckErrorType(R"({
    "width": 90,
    "showTitleRow": true,
    "columns": [{
        "title": "Error ID",
        "size": 10
    }, {
        "title": "Error Type",
        "size": 33
    }, {
        "title": "Error Category",
        "size": 20
    }, {
        "title": "Error Severity"
    }],
    "rows": [{
        "instance": "error_type_list[]",
        "cells": [
            { "value": "error_id" },
            { "value": "error_type" },
            { "value": "error_category" },
            { "value": "error_severity" }
        ]
    }]
})"_json);

std::unordered_map<int, int> testIdToType = {{1, XPUM_DIAG_PERFORMANCE_COMPUTATION}, 
                                                {2, XPUM_DIAG_MEMORY_ERROR}, 
                                                {3, XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH}, 
                                                {4, XPUM_DIAG_MEDIA_CODEC}, 
                                                {5, XPUM_DIAG_INTEGRATION_PCIE}, 
                                                {6, XPUM_DIAG_PERFORMANCE_POWER},
                                                {7, XPUM_DIAG_LIGHT_COMPUTATION},
                                                {8, XPUM_DIAG_LIGHT_CODEC},
                                                {9, XPUM_DIAG_XE_LINK_THROUGHPUT},
                                                {10,XPUM_DIAG_XE_LINK_ALL_TO_ALL_THROUGHPUT}};

std::unique_ptr<nlohmann::json> ComletDiagnostic::deviceOptToId(int &deviceId, 
    const std::string &deviceOpt) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (deviceOpt == "-1") {
        deviceId = -1;
        return json;
    }
    if (isNumber(deviceOpt) == true) {
        deviceId = std::stoi(deviceOpt);
        return json;        
    }
    return this->coreStub->getDeivceIdByBDF(deviceOpt.c_str(), &deviceId);
}

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
    std::string errStr = "Device id should be an integer or a BDF string";
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
    auto preCheckOpt = addFlag("--precheck", this->opts->preCheck, "Do the precheck on the GPU and GPU driver. By default, precheck scans kernel messages by journalctl.\n\
It could be configured to scan dmesg or log file through xpum.conf.");
    auto listErrorTypeOpt = addFlag("--listtypes", this->opts->listErrorType, "List all supported GPU error types");
    auto onlyGPUOpt = addFlag("--gpu", this->opts->onlyGPU, "Show the GPU status only");
    auto sinceTimeOpt = addOption("--since", this->opts->sinceTime, "Start time for log scanning. It only works with the journalctl option. The generic format is \"YYYY-MM-DD HH:MM:SS\".\n\
Alternatively the strings \"yesterday\", \"today\" are also understood.\n\
Relative times also may be specified, prefixed with \"-\" referring to times before the current time.\n\
Scanning would start from the latest boot if it is not specified.");

std::string singleTestIdListDesc = "Selectively run some particular tests. Separated by the comma.\n\
       1. Computation\n\
       2. Memory Error\n\
       3. Memory Bandwidth\n\
       4. Media Codec\n\
       5. PCIe Bandwidth\n\
       6. Power\n\
       7. Computation functional test\n\
       8. Media Codec functional test\n\
       9. Xe Link Throughput\n\
      10. Xe Link all-to-all Throughput. It only works for all GPUs (\"-d -1\")";
#ifdef DAEMONLESS  
    singleTestIdListDesc += "\nNote that in a multi NUMA node server, it may need to use numactl to specify which node the PCIe bandwidth test runs on.\n\
Usage: numactl [ --membind nodes ] [ --cpunodebind nodes ] xpu-smi diag -d [deviceId] --singletest 5\n\
It also applies to diag level tests.";
#endif
    auto singleTestIdList = addOption("--singletest", this->opts->singleTestIdList, singleTestIdListDesc);  
    singleTestIdList->delimiter(',');
    singleTestIdList->check(CLI::Range(1, (int)testIdToType.size()));

    preCheckOpt->excludes(deviceIdOpt);
    preCheckOpt->excludes(level);
    preCheckOpt->excludes(stressFlag);
    preCheckOpt->excludes(stressTimeOpt);
    level->excludes(preCheckOpt);
    level->excludes(stressFlag);
    level->excludes(stressTimeOpt);
    level->excludes(singleTestIdList);
    singleTestIdList->excludes(level);
    sinceTimeOpt->needs(preCheckOpt);

    deviceIdOpt->excludes(preCheckOpt);
    stressTimeOpt->needs(stressFlag);

    onlyGPUOpt->needs(preCheckOpt);

    listErrorTypeOpt->needs(preCheckOpt);
#ifndef DAEMONLESS
    preCheckOpt->excludes(groupIdOpt);
    groupIdOpt->excludes(preCheckOpt);
    groupIdOpt->excludes(stressFlag);
    groupIdOpt->excludes(stressTimeOpt);
    stressFlag->needs(stressTimeOpt);
#endif
}

bool ComletDiagnostic::isPreCheck() {
    return this->opts->preCheck;
}


void ComletDiagnostic::getJsonResult(std::ostream &out, bool raw) {
    if (this->opts->stress) {
        out << "Not supported" << std::endl;
        return;
    } else {
        ComletBase::getJsonResult(out, raw);
    }
};

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

    if (this->opts->singleTestIdList.size() > 0) {
        // check singletest is unique
        std::set<int> idSet(this->opts->singleTestIdList.begin(), this->opts->singleTestIdList.end());
        if (idSet.size() != this->opts->singleTestIdList.size()) {
            json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
            (*json)["error"] = "Duplicated single test";
            (*json)["errno"] = XPUM_CLI_ERROR_DIAGNOSTIC_DUPLICATED_SINGLE_TEST;
            return json;
        }
        for (std::size_t i = 0; i < this->opts->singleTestIdList.size(); i++) {
            if (this->opts->singleTestIdList[i] < 1 || this->opts->singleTestIdList[i] > (int)testIdToType.size()) {
                (*json)["error"] = "invalid single test";
                (*json)["errno"] = XPUM_CLI_ERROR_DIAGNOSTIC_INVALID_SINGLE_TEST;
                return json;
            }
        }
        // XPUM_DIAG_XE_LINK_ALL_TO_ALL_THROUGHPUT only works for all GPUs
        if (idSet.find(10) != idSet.end() && this->opts->deviceId != "-1") {
            (*json)["error"] = "invalid single test";
            (*json)["errno"] = XPUM_CLI_ERROR_DIAGNOSTIC_INVALID_SINGLE_TEST;
            return json;            
        }
    }

    int deviceId = -1;
    auto ret = deviceOptToId(deviceId, this->opts->deviceId);
    if (ret->contains("error") == true) {
        return ret;
    }

    if (this->opts->level >= 1 && this->opts->level <= 3) {
#ifndef DAEMONLESS
        if (this->opts->groupId > 0 && this->opts->groupId != UINT_MAX) {
            json = this->coreStub->runDiagnosticsByGroup(this->opts->groupId, this->opts->level, {}, this->opts->rawJson);
            return json;
        }
#endif
        if (this->opts->deviceId != "-1") {
            json = this->coreStub->runDiagnostics(deviceId, this->opts->level, {}, this->opts->rawJson);
            return json;
        } else {
            auto deviceListJson = this->coreStub->getDeviceList();
            auto deviceList = (*deviceListJson)["device_list"];
            std::string ids;
            std::vector<std::string> opt_ids;
            for (auto device : deviceList) {
                ids = ids.empty() ? to_string(device["device_id"]) : ids + ", " + to_string(device["device_id"]);
                opt_ids.push_back(to_string(device["device_id"]));
            }
            // run level diagnostics on all GPUs
            json = this->coreStub->runDiagnostics(-1, this->opts->level, {}, this->opts->rawJson);
            if (json->contains("error") && (*json)["errno"] != XPUM_CLI_ERROR_DIAGNOSTIC_TASK_FAILED)
                return json;
            if (json->contains("device_id"))
                (*json)["device_id"] = ids;
            return json;
        }
    }

    if (this->opts->singleTestIdList.size() > 0) {
        std::vector<int> targetTypes;
        for (auto testId : this->opts->singleTestIdList)
            targetTypes.push_back(testIdToType[testId]);
#ifndef DAEMONLESS
        if (this->opts->groupId > 0 && this->opts->groupId != UINT_MAX) {
            json = this->coreStub->runDiagnosticsByGroup(this->opts->groupId, -1, targetTypes, this->opts->rawJson);
            return json;
        }
#endif
        if (this->opts->deviceId != "-1") {
            json = this->coreStub->runDiagnostics(deviceId, -1, targetTypes, this->opts->rawJson);
            return json;
        } else {
            auto deviceListJson = this->coreStub->getDeviceList();
            auto deviceList = (*deviceListJson)["device_list"];
            std::string ids;
            std::vector<std::string> opt_ids;
            for (auto device : deviceList) {
                ids = ids.empty() ? to_string(device["device_id"]) : ids + ", " + to_string(device["device_id"]);
                opt_ids.push_back(to_string(device["device_id"]));
            }
            // run singletest diagnostics on all GPUs 
            json = this->coreStub->runDiagnostics(-1, -1, targetTypes, this->opts->rawJson);
            if (json->contains("error") && (*json)["errno"] != XPUM_CLI_ERROR_DIAGNOSTIC_TASK_FAILED)
                return json;
            if (json->contains("device_id"))
                (*json)["device_id"] = ids;
            return json;
        }
    }

    if (this->opts->preCheck) {
        if (this->opts->listErrorType) {
            json = this->coreStub->getPrecheckErrorTypes();
        } else {
            xpum_precheck_options options = { this->opts->onlyGPU, this->opts->sinceTime.c_str()};
            json = this->coreStub->precheck(options, this->opts->rawJson);
        }
        return json;
    }

    if (this->opts->stress) {
        return this->coreStub->runStress(deviceId, this->opts->stressTime);
    }

    (*json)["error"] = "Wrong argument or unknown operation, run with --help for more information.";
    (*json)["errno"] = XPUM_CLI_ERROR_BAD_ARGUMENT;
    return json;
}

static void showDeviceDiagnostic(std::ostream &out, std::shared_ptr<nlohmann::json> json, ShowMode mode, const bool cont = false) {
    if (mode == PRE_CHECK) {
        CharTable table(ComletConfigDiagnosticPreCheck, *json, cont);
        table.show(out);
    } else if (mode == PRE_CHECK_ERROR_TYPE) {
        CharTable table(ComletConfigDiagnosticPreCheckErrorType, *json, cont);
        table.show(out); 
    } else if (mode == SINGLE_TEST) {
        CharTable table(ComletConfigSpecificDiagnosticDevice, *json, cont);
        table.show(out);
    } else {
        CharTable table(ComletConfigDiagnosticDevice, *json, cont);
        table.show(out);
    }
}

void ComletDiagnostic::getTableResult(std::ostream &out) {
    this->opts->rawJson = false;
    auto res = run();
    this->opts->rawJson = true;
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
            if (this->opts->level >= 1 && this->opts->level <= 3)
                showDeviceDiagnostic(out, std::make_shared<nlohmann::json>(device), LEVEL_TEST, cont);
            else
                showDeviceDiagnostic(out, std::make_shared<nlohmann::json>(device), SINGLE_TEST, cont);
            cont = true;
        }
        return;
    }
#endif

    if (this->opts->stress) {
        int deviceId = -1;
        auto ret = deviceOptToId(deviceId, this->opts->deviceId);
        if (ret->contains("error") == true) {
            out << "Error: " << 
                (*json)["error"].get<std::string>() << std::endl;
            setExitCodeByJson(*json);
            return;
        }
        out << "Started to stress GPU and would update the status in every minute" << std::endl;
        uint64_t round = 0;
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
            json = this->coreStub->checkStress(deviceId);
            if (json->contains("error")) {
                out << "Error: " << 
                    (*json)["error"].get<std::string>() << std::endl;
                setExitCodeByJson(*json);
                return;
            }
            round++;
            auto tasks = (*json)["task_list"];
            if (tasks == nullptr || tasks.size() == 0) {
                out << "Error: stress task list not found." << std::endl;
                exit_code = XPUM_CLI_ERROR_GENERIC_ERROR;
                return;
            }
            out << "Stress on GPU: ";
            size_t fin_task_count = 0;
            size_t i = 0;
            for (auto task : tasks) {
                out << task["device_id"];
                if (i != tasks.size() - 1) {
                    out << ",";
                }
                bool finished = task["finished"];
                if (finished) {
                    fin_task_count++;
                }
                i++;
            }
            out << "; Round " << round << "; " << 
                tasks[0]["message"].get<std::string>() << std::endl;
            if (tasks.size() == fin_task_count) {
                out << "Finish stressing." << std::endl;
                break;   
            }
        }
        return;
    }

    if (this->opts->level >= 1 && this->opts->level <= 3) {
        showDeviceDiagnostic(out, json, LEVEL_TEST);
        return;
    }

    if (this->opts->singleTestIdList.size() > 0) {
        showDeviceDiagnostic(out, json, SINGLE_TEST);
        return;
    }

    if (this->opts->preCheck) {
        if (this->opts->listErrorType)
            showDeviceDiagnostic(out, json, PRE_CHECK_ERROR_TYPE);
        else
            showDeviceDiagnostic(out, json, PRE_CHECK);
        return;
    }
}
} // end namespace xpum::cli
