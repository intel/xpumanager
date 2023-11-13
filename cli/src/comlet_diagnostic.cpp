/* 
 *  Copyright (C) 2021-2023 Intel Corporation
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
                                                {9, XPUM_DIAG_XE_LINK_THROUGHPUT}};

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
    auto preCheckOpt = addFlag("--precheck", this->opts->preCheck, "Do the precheck on the GPU and GPU driver. By default, precheck scans kernel messages by journalctl.\n\
It could be configured to scan dmesg or log file through diagnostics.conf.");
    auto listErrorTypeOpt = addFlag("--listtypes", this->opts->listErrorType, "List all supported GPU error types");
    auto onlyGPUOpt = addFlag("--gpu", this->opts->onlyGPU, "Show the GPU status only");
    auto sinceTimeOpt = addOption("--since", this->opts->sinceTime, "Start time for log scanning. It only works with the journalctl option. The generic format is \"YYYY-MM-DD HH:MM:SS\".\n\
Alternatively the strings \"yesterday\", \"today\" are also understood.\n\
Relative times also may be specified, prefixed with \"-\" referring to times before the current time.\n\
Scanning would start from the latest boot if it is not specified.");

    auto singleTestIdList = addOption("--singletest", this->opts->singleTestIdList,
              "Selectively run some particular tests. Separated by the comma.\n\
      1. Computation\n\
      2. Memory Error\n\
      3. Memory Bandwidth\n\
      4. Media Codec\n\
      5. PCIe Bandwidth\n\
      6. Power\n\
      7. Computation functional test\n\
      8. Media Codec functional test\n\
      9. Xe Link Throughput");
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

std::unordered_map<std::string, std::string> rawComponentTypeToString = {
    {"XPUM_DIAG_SOFTWARE_ENV_VARIABLES", "Software Env Variables"},
    {"XPUM_DIAG_SOFTWARE_LIBRARY", "Software Library"},
    {"XPUM_DIAG_SOFTWARE_PERMISSION", "Software Permission"},
    {"XPUM_DIAG_SOFTWARE_EXCLUSIVE", "Software Exclusive"},
    {"XPUM_DIAG_HARDWARE_SYSMAN", "Hardware Sysman"},
    {"XPUM_DIAG_LIGHT_COMPUTATION", "Computation Check"},
    {"XPUM_DIAG_LIGHT_CODEC",  "Media Codec Check"},
    {"XPUM_DIAG_INTEGRATION_PCIE", "Integration PCIe"},
    {"XPUM_DIAG_MEDIA_CODEC", "Media Codec"},
    {"XPUM_DIAG_PERFORMANCE_COMPUTATION", "Performance Computation"},
    {"XPUM_DIAG_PERFORMANCE_POWER", "Performance Power"},
    {"XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION", "Performance Memory Allocation"},
    {"XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH",  "Performance Memory Bandwidth"},
    {"XPUM_DIAG_MEMORY_ERROR", "Memory Error"},
    {"XPUM_DIAG_XE_LINK_THROUGHPUT", "Xe Link Throughput"}
};

// combine multiple devices' diagnostic result
std::unique_ptr<nlohmann::json> ComletDiagnostic::combineDeviceData(CombinedDiagResult& cr, std::shared_ptr<nlohmann::json> one) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (cr.combined_device_ids.empty()) {
        cr.combined_device_ids = to_string((*one)["device_id"]);
        cr.combined_start_time = (*one)["start_time"];
        cr.combined_result = (*one)["result"];
    }  else {
        cr.combined_device_ids = cr.combined_device_ids + ", " + to_string((*one)["device_id"]);
        if ((*one)["result"] != "Pass") {
            cr.combined_result = (*one)["result"];
        }
    }
    (*json)["device_id"] = cr.combined_device_ids;
    (*json)["start_time"] = cr.combined_start_time;
    (*json)["result"] = cr.combined_result;

    if ((*json)["result"] != "Pass")
        (*json)["errno"] = XPUM_CLI_ERROR_DIAGNOSTIC_TASK_FAILED;
    (*json)["end_time"] = (*one)["end_time"];
    (*json)["finished"] = (*one)["finished"];
    if (one->contains("level")) {
        (*json)["level"] = (*one)["level"];
    }
    (*json)["message"] = (*one)["message"];
    (*json)["component_count"] = (*one)["component_count"];
    std::vector<nlohmann::json> componentJsonList;
    for (auto& component : (*one)["component_list"]) {
        std::string ctype = component["component_type"];
        auto componentJson = nlohmann::json();
        componentJson["component_type"] = this->opts->rawJson ? ctype : rawComponentTypeToString[ctype];
        componentJson["finished"] = component["finished"];
        if (component["result"] != "Pass") {
            cr.combinedComponentTypeResultMessages[ctype].result = "Fail";
        }
        componentJson["result"] = cr.combinedComponentTypeResultMessages[ctype].result;

        std::string message = component["message"];
        // combine exclusive result
        if (ctype == "XPUM_DIAG_SOFTWARE_EXCLUSIVE") {
            if (component.contains("process_list")) {
                cr.combinedComponentTypeResultMessages[ctype].message += "\n GPU " +  to_string((*one)["device_id"]) + ": " + message;
                for (auto process : component["process_list"]) {
                    std::string command_name = process["process_name"];
                    cr.combinedComponentTypeResultMessages[ctype].message += "\n  PID: " +  to_string(process["process_id"]) + " Command: " + command_name;
                }
            }
            if (cr.combinedComponentTypeResultMessages[ctype].message.find("PID") == std::string::npos)
                componentJson["message"] = componentJson["message"] = cr.combinedComponentTypeResultMessages[ctype].result + cr.combinedComponentTypeResultMessages[ctype].message;
            else
                componentJson["message"] = "Warnning when" + cr.combinedComponentTypeResultMessages[ctype].message.substr(3);
        }
        if (component["result"] != "Pass") {
            if (ctype == "XPUM_DIAG_LIGHT_COMPUTATION"
                || ctype == "XPUM_DIAG_LIGHT_CODEC"
                || ctype == "XPUM_DIAG_MEDIA_CODEC"
                || ctype == "XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION") {
                cr.combinedComponentTypeResultMessages[ctype].message += "\n GPU " +  to_string((*one)["device_id"]) + ": " + message;
            }

            // combine performance data
            if (ctype == "XPUM_DIAG_INTEGRATION_PCIE"
                || ctype == "XPUM_DIAG_PERFORMANCE_COMPUTATION"
                || ctype == "XPUM_DIAG_PERFORMANCE_POWER"
                || ctype == "XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH") {
                message = message.substr(message.find("Its"));
                cr.combinedComponentTypeResultMessages[ctype].message += "\n GPU " +  to_string((*one)["device_id"]) + ": " + message;
            }

            // combine memory error info
            if (ctype == "XPUM_DIAG_MEMORY_ERROR") {
                std::string head = "Fail to check memory error. ";
                message = message.substr(head.size());
                cr.combinedComponentTypeResultMessages[ctype].message += "\n GPU " +  to_string((*one)["device_id"]) + ": " + message;
            }

            // combine failed xe link throughput
            if (ctype == "XPUM_DIAG_XE_LINK_THROUGHPUT") {
                cr.combinedComponentTypeResultMessages[ctype].message += "\n GPU " +  to_string((*one)["device_id"]) + ": " + message;
                if (component.contains("xe_link_throughput_list")) {
                    for (auto xeLinkThroughput : component["xe_link_throughput_list"]) {
                        std::string line = "  GPU " + to_string(xeLinkThroughput["src_device_id"]) + "/" 
                                        + to_string(xeLinkThroughput["src_tile_id"]) + " port " + to_string(xeLinkThroughput["src_port_id"]) 
                                        + " to GPU " + to_string(xeLinkThroughput["dst_device_id"]) + "/" 
                                        + to_string(xeLinkThroughput["dst_tile_id"]) + " port " + to_string(xeLinkThroughput["dst_port_id"]) 
                                        + ": " + roundDouble(xeLinkThroughput["current_speed"], 3) + " GBPS. Threshold: " + roundDouble(xeLinkThroughput["threshold"], 3) + " GBPS.";                            
                        cr.combinedComponentTypeResultMessages[ctype].message += "\n" + line;
                    }
                }
            }
        }
        // exception for exclusive because its result is "Pass" by design
        if (ctype != "XPUM_DIAG_SOFTWARE_EXCLUSIVE")
            componentJson["message"] = cr.combinedComponentTypeResultMessages[ctype].result + cr.combinedComponentTypeResultMessages[ctype].message;
        componentJsonList.push_back(componentJson);
          
    }
    (*json)["component_list"] = componentJsonList;
    return json;  
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
    }

    if (this->opts->level >= 1 && this->opts->level <= 3) {
#ifndef DAEMONLESS
        if (this->opts->groupId > 0 && this->opts->groupId != UINT_MAX) {
            json = this->coreStub->runDiagnosticsByGroup(this->opts->groupId, this->opts->level, {}, this->opts->rawJson);
            return json;
        }
#endif
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
            json = this->coreStub->runDiagnostics(targetId, this->opts->level, {}, this->opts->rawJson);
            return json;
        } else {
            auto deviceListJson = this->coreStub->getDeviceList();
            auto deviceList = (*deviceListJson)["device_list"];
            std::vector<std::string> ids;
            CombinedDiagResult cr;
            // run level diagnostics on all GPUs
            for (auto device : deviceList) {
                ids.push_back(to_string(device["device_id"]));
                auto jsonTmp = this->coreStub->runDiagnostics(device["device_id"], this->opts->level, {}, true);
                if (jsonTmp->contains("error") && (*jsonTmp)["errno"] != XPUM_CLI_ERROR_DIAGNOSTIC_TASK_FAILED) {
                    return jsonTmp;
                }
                json = combineDeviceData(cr, std::make_shared<nlohmann::json>(*jsonTmp));
            }
            this->opts->deviceIds = ids;
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
            json = this->coreStub->runDiagnostics(targetId, -1, targetTypes, this->opts->rawJson);
            return json;
        } else {
            auto deviceListJson = this->coreStub->getDeviceList();
            auto deviceList = (*deviceListJson)["device_list"];
            std::vector<std::string> ids;
            // run singletest diagnostics on all GPUs 
            CombinedDiagResult cr;       
            for (auto device : deviceList) {
                ids.push_back(to_string(device["device_id"]));
                auto jsonTmp = this->coreStub->runDiagnostics(device["device_id"], -1, targetTypes, true);
                 if (jsonTmp->contains("error") && (*jsonTmp)["errno"] != XPUM_CLI_ERROR_DIAGNOSTIC_TASK_FAILED) {
                    return jsonTmp;
                }
                json = combineDeviceData(cr, std::make_shared<nlohmann::json>(*jsonTmp));
            }
            this->opts->deviceIds = ids;
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
        if (this->opts->level >= 1 && this->opts->level <= 3)
            showDeviceDiagnostic(out, json, LEVEL_TEST);
        else
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
