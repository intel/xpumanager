/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_statistics.cpp
 */

#include "comlet_statistics.h"

#include <map>
#include <nlohmann/json.hpp>

#include "cli_table.h"
#include "core_stub.h"
#include "exit_code.h"
#include "utility.h"

namespace xpum::cli {

static CharTableConfig ComletConfigDeviceStatistics(R"({
    "showTitleRow": false,
    "columns": [{
        "title": "none",
        "size": 27
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
            { "rowTitle": "Average % utilization of all GPU Engines " },
            { "rowTitle": "EU Array Active (%) " },
            { "rowTitle": "EU Array Stall (%) " },
            { "rowTitle": "EU Array Idle (%) " },
            { "rowTitle": " " },
            { "rowTitle": "Compute Engine Util (%) " },
            { "rowTitle": "Render Engine Util (%) " },
            { "rowTitle": "Media Engine Util (%) " },
            { "rowTitle": "Decoder Engine Util (%) " },
            { "rowTitle": "Encoder Engine Util (%) " },
            { "rowTitle": "Copy Engine Util (%) " },
            { "rowTitle": "Media EM Engine Util (%) " },
            { "rowTitle": "3D Engine Util (%) " }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_GPU_UTILIZATION].value", "fixer": "round" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_ACTIVE].value", "scale": 1 }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_STALL].value", "scale": 1 }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_IDLE].value", "scale": 1 }
            ]},
            { "value": " "},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION].value", "fixer": "round" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION].value", "fixer": "round" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION].value", "fixer": "round" }
            ]},
            { "value": "decoder_engine_util"},
            { "value": "encoder_engine_util"},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION].value", "fixer": "round" }
            ]},
            { "value": "media_em_engine_util"},
            { "value": "3d_engine_util"}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "Reset" },
            { "rowTitle": "Programming Errors" },
            { "rowTitle": "Driver Errors" },
            { "rowTitle": "Cache Errors Correctable" },
            { "rowTitle": "Cache Errors Uncorrectable" },
            { "rowTitle": "Mem Errors Correctable" },
            { "rowTitle": "Mem Errors Uncorrectable" }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_RESET].value" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS].value" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS].value" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE].value" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE].value" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE].value" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE].value" }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "GPU Power (W) " },
            { "rowTitle": "GPU Frequency (MHz) " },
            { "rowTitle": "Media Engine Freq (MHz) " },
            { "rowTitle": "GPU Core Temperature (C) " },
            { "rowTitle": "GPU Memory Temperature (C) " },
            { "rowTitle": "GPU Memory Read (kB/s) " },
            { "rowTitle": "GPU Memory Write (kB/s) " },
            { "rowTitle": "GPU Memory Bandwidth (%) " },
            { "rowTitle": "GPU Memory Used (MiB) " },
            { "rowTitle": "GPU Memory Util (%) " },
            { "rowTitle": "Xe Link Throughput (kB/s) " }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_POWER].value", "fixer": "round" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_GPU_FREQUENCY].value" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEDIA_ENGINE_FREQUENCY].value" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_GPU_CORE_TEMPERATURE].value", "fixer": "round" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_TEMPERATURE].value", "fixer": "round" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_READ_THROUGHPUT].value", "fixer": "round" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_WRITE_THROUGHPUT].value", "fixer": "round" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_BANDWIDTH].value" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_USED].value", "scale": 1, "fixer": "round" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_UTILIZATION].value", "fixer": "round" }
            ]}, { "value": "fabric_throughput"}
        ]]
    }]
})"_json);
static CharTableConfig ComletConfigDeviceStatisticsDeviceLevel(R"({
    "showTitleRow": false,
    "columns": [{
        "title": "none",
        "size": 27
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
            { "rowTitle": "Average % utilization of all GPU Engines " },
            { "rowTitle": "EU Array Active (%) " },
            { "rowTitle": "EU Array Stall (%) " },
            { "rowTitle": "EU Array Idle (%) " },
            { "rowTitle": " " },
            { "rowTitle": "Compute Engine Util (%) " },
            { "rowTitle": "Render Engine Util (%) " },
            { "rowTitle": "Media Engine Util (%) " },
            { "rowTitle": "Decoder Engine Util (%) " },
            { "rowTitle": "Encoder Engine Util (%) " },
            { "rowTitle": "Copy Engine Util (%) " },
            { "rowTitle": "Media EM Engine Util (%) " },
            { "rowTitle": "3D Engine Util (%) " }
        ], [
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_GPU_UTILIZATION].value", "fixer": "round" }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_ACTIVE].value", "scale": 1 }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_STALL].value", "scale": 1 }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_IDLE].value", "scale": 1 }
            ]},
            { "value": " "},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION].value", "fixer": "round" }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION].value", "fixer": "round" }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION].value", "fixer": "round" }
            ]},
            { "value": "decoder_engine_util"},
            { "value": "encoder_engine_util"},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION].value", "fixer": "round" }
            ]},
            { "value": "media_em_engine_util"},
            { "value": "3d_engine_util"}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "Reset" },
            { "rowTitle": "Programming Errors" },
            { "rowTitle": "Driver Errors" },
            { "rowTitle": "Cache Errors Correctable" },
            { "rowTitle": "Cache Errors Uncorrectable" },
            { "rowTitle": "Mem Errors Correctable" },
            { "rowTitle": "Mem Errors Uncorrectable" }
        ], [
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_RESET].value" }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS].value" }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS].value" }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE].value" }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE].value" }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE].value" }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE].value" }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "GPU Power (W) " },
            { "rowTitle": "GPU Frequency (MHz) " },
            { "rowTitle": "Media Engine Freq (MHz) " },
            { "rowTitle": "GPU Core Temperature (C) " },
            { "rowTitle": "GPU Memory Temperature (C) " },
            { "rowTitle": "GPU Memory Read (kB/s) " },
            { "rowTitle": "GPU Memory Write (kB/s) " },
            { "rowTitle": "GPU Memory Bandwidth (%) " },
            { "rowTitle": "GPU Memory Used (MiB) " },
            { "rowTitle": "GPU Memory Util (%) " },
            { "rowTitle": "Xe Link Throughput (kB/s) " }
        ], [
            { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_POWER].value", "fixer": "round" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_GPU_FREQUENCY].value" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEDIA_ENGINE_FREQUENCY].value" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_GPU_CORE_TEMPERATURE].value", "fixer": "round" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_TEMPERATURE].value", "fixer": "round" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_READ_THROUGHPUT].value", "fixer": "round" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_WRITE_THROUGHPUT].value", "fixer": "round" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_BANDWIDTH].value" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_USED].value", "scale": 1, "fixer": "round" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_UTILIZATION].value", "fixer": "round" }
            ]}, { "value": "fabric_throughput"}
        ]]
    }]
})"_json);

bool ComletStatistics::hasEUMetrics() {
    return this->opts->showEuMetrics;
}

bool ComletStatistics::hasRASMetrics() {
    return this->opts->showRASMetrics;
}

void ComletStatistics::setupOptions() {
    this->opts = std::unique_ptr<ComletStatisticsOptions>(new ComletStatisticsOptions());
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceId, "The device ID or PCI BDF address to query");
    addFlag("-e,--eu", this->opts->showEuMetrics, "Show EU metrics");

    deviceIdOpt->check([this](const std::string& str) {
        std::string errStr = "Device ID should be a non-negative integer or a BDF string";
        if (isValidDeviceId(str)) {
            return std::string();
        } else if (isBDF(str)) {
            return std::string();
        }
        return errStr;
    });
}

std::unique_ptr<nlohmann::json> ComletStatistics::run() {
    if (isDeviceOp()) {
        int targetId = -1;
        if (isNumber(this->opts->deviceId)) {
            targetId = std::stoi(this->opts->deviceId);
        } else {
            auto convertResult = this->coreStub->getDeivceIdByBDF(this->opts->deviceId.c_str(), &targetId);
            if (convertResult->contains("error")) {
                return convertResult;
            }
        }
        auto json = this->coreStub->getRealtimeMetrics(targetId, true);
        return json;
    } 
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    (*json)["error"] = "Unknow operation";
    return json;
}

std::string engineUtilFormater(nlohmann::json json, bool intent = false) {
    std::string intent_str = intent ? std::string(2, ' ') : "";
    std::string tmp;
    std::string res;
    int i = 0;
    auto func = [](nlohmann::json obj1, nlohmann::json obj2) {
        return obj1["engine_id"] <= obj2["engine_id"];
    };
    std::sort(json.begin(), json.end(), func);
    for (auto obj : json) {
        if (tmp.empty()) tmp = intent_str;
        tmp += "Engine " + std::to_string(obj["engine_id"].get<int>()) + ": " + std::to_string(obj["value"].get<int>());
        i++;
        if (i % 4 == 0) {
            res += tmp + "\n";
            tmp.clear();
        } else {
            tmp += ", ";
        }
    }
    if (!tmp.empty()) {
        res += tmp;
        res.pop_back();
        res.pop_back();
    } else if (i > 0) {
        res.pop_back();
    }
    return res;
}

std::string engineUtilByType(std::shared_ptr<nlohmann::json> jsonPtr, std::string key) {
    std::string res;
    auto findEngineGroupUtilFn = [key](nlohmann::json item) {
        std::string tmpKey(key);
        std::transform(tmpKey.begin(), tmpKey.end(), tmpKey.begin(), toupper);
        tmpKey += "_ALL_UTILIZATION";
        return item["metrics_type"].get<std::string>().find(tmpKey) != std::string::npos;
    };

    // device level
    if (jsonPtr->contains("engine_util") && (*jsonPtr)["engine_util"].contains(key)) {
        auto jsonObj = (*jsonPtr)["engine_util"][key];
        auto found = std::find_if(
            (*jsonPtr)["device_level"].begin(),
            (*jsonPtr)["device_level"].end(),
            findEngineGroupUtilFn);
        if (found != (*jsonPtr)["device_level"].end()) {
            res += std::to_string((*found)["value"].get<int>()) + "; ";
        }
        res += engineUtilFormater(jsonObj) + "\n";
    }

    // tile level
    if (jsonPtr->contains("tile_level")) {
        for (auto tileJson : (*jsonPtr)["tile_level"]) {
            if (tileJson.contains("engine_util") && tileJson["engine_util"].contains(key)) {
                auto jsonObj = tileJson["engine_util"][key];
                auto engineStr = engineUtilFormater(jsonObj, true);
                auto found = std::find_if(
                    tileJson["data_list"].begin(),
                    tileJson["data_list"].end(),
                    findEngineGroupUtilFn);
                if (!engineStr.empty()) {
                    res += "Tile " + std::to_string(tileJson["tile_id"].get<int>()) + ":\n";
                    if (found != tileJson["data_list"].end()) {
                        res += "  " + std::to_string((*found)["value"].get<int>()) + "; " + engineUtilFormater(jsonObj, false) + "\n";
                    } else {
                        res += engineUtilFormater(jsonObj, true) + "\n";
                    }

                }
            }
        }
    }

    if (!res.empty()) res.pop_back();
    return res;
}

std::string getXelinkThroughput(std::shared_ptr<nlohmann::json> jsonPtr) {
    std::string res;
    if (!jsonPtr->contains("fabric_throughput"))
        return res;
    std::stringstream ss;
    for (auto& obj : (*jsonPtr)["fabric_throughput"]) {
        ss.str("");
        auto key = obj["name"].get<std::string>();
        int i = key.find("->");
        key.insert(i + 2, " ");
        key.insert(i, " ");
        ss << key << ": ";
        ss << obj["value"];
        res += ss.str() + "\n";
    }
    if (!res.empty()) res.pop_back();
    return res;
}

static void showDeviceStatistics(std::ostream& out, std::shared_ptr<nlohmann::json> jsonPtr, const bool cont = false) {
    nlohmann::json& json = *jsonPtr;
    bool noTile = true;
    auto tileJson = json.find("tile_level");
    if (tileJson != json.end()) {
        if (tileJson->is_array() && tileJson->size() >= 2) {
            noTile = false;
        }
    }

    if (noTile) {
        auto deviceJson = json.find("device_level");
        if (deviceJson != json.end() && deviceJson->is_array()) {
            if (tileJson != json.end()) {
                json.erase(tileJson);
            }
            json["tile_level"] = nlohmann::json::array();
            tileJson = json.find("tile_level");
            auto tile0 = nlohmann::json::object();
            tile0["tile_id"] = 0;
            tile0["data_list"] = (*deviceJson);
            tileJson->push_back(tile0);
        }
    }

    json["compute_engine_util"] = engineUtilByType(jsonPtr, "compute");
    json["render_engine_util"] = engineUtilByType(jsonPtr, "render");
    json["decoder_engine_util"] = engineUtilByType(jsonPtr, "decoder");
    json["encoder_engine_util"] = engineUtilByType(jsonPtr, "encoder");
    json["copy_engine_util"] = engineUtilByType(jsonPtr, "copy");
    json["media_em_engine_util"] = engineUtilByType(jsonPtr, "media_enhancement");
    json["3d_engine_util"] = engineUtilByType(jsonPtr, "3d");

    json["fabric_throughput"] = getXelinkThroughput(jsonPtr);

    CharTable table(noTile ? ComletConfigDeviceStatisticsDeviceLevel : ComletConfigDeviceStatistics, json, cont);
    table.show(out);
}

void ComletStatistics::getTableResult(std::ostream& out) {
    auto res = run();
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        setExitCodeByJson(*res);
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;
    showDeviceStatistics(out, json);
}

} // end namespace xpum::cli
