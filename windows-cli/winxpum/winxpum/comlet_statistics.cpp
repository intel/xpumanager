/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_statistics.cpp
 */

#include "comlet_statistics.h"

#include <map>
#include <nlohmann/json.hpp>

#include "core_stub.h"
#include "cli_table.h"


static CharTableConfig ComletConfigDeviceStatistics(R"({
"showTitleRow": false,
"columns": [{
    "title": "none",
    "size": 26
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
        { "rowTitle": "Start Time" },
        { "rowTitle": "End Time" },
        { "rowTitle": "Elapsed Time (Second) " },
        { "rowTitle": "Energy Consumed (J) " },
        { "rowTitle": "GPU Utilization (%) " },
        { "rowTitle": "EU Array Active (%) " },
        { "rowTitle": "EU Array Stall (%) " },
        { "rowTitle": "EU Array Idle (%) " }
    ], [
        { "value": "begin" },
        { "value": "end" },
        { "value": "elapsed_time", "scale": 1000 },
        { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
            { "value": "data_list[metrics_type==XPUM_STATS_ENERGY].value", "scale": 1000 }
        ]},
        { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
            { "value": "data_list[metrics_type==XPUM_STATS_GPU_UTILIZATION].value", "fixer": "round" }
        ]},
        { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
            { "value": "data_list[metrics_type==XPUM_STATS_EU_ACTIVE].value" }
        ]},
        { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
            { "value": "data_list[metrics_type==XPUM_STATS_EU_STALL].value" }
        ]},
        { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
            { "value": "data_list[metrics_type==XPUM_STATS_EU_IDLE].value" }
        ]}
    ]]
}, {
    "instance": "",
    "cells": [[
        { "rowTitle": "Reset" },
        { "rowTitle": "Programming Errors" },
        { "rowTitle": "Driver Errors" },
        { "rowTitle": "Cache Errors Correctable" },
        { "rowTitle": "Cache Errors Uncorrectable" }
    ], [
        { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
            { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_RESET].value" },
            { "label": "total", "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_RESET].total" }
        ]},
        { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
            { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS].value" },
            { "label": "total", "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS].total" }
        ]},
        { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
            { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS].value" },
            { "label": "total", "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS].total" }
        ]},
        { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
            { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE].value" },
            { "label": "total", "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE].total" }
        ]},
        { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
            { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE].value" },
            { "label": "total", "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE].total" }
        ]}
    ]]
}, {
    "instance": "",
    "cells": [[
        { "rowTitle": "GPU Power (W) " }
    ], [
        { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
            { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_POWER].avg", "fixer": "round" },
            { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_POWER].min", "fixer": "round" },
            { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_POWER].max", "fixer": "round" },
            { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_POWER].value", "fixer": "round" }
        ]}
    ]]
}, {
    "instance": "",
    "cells": [[
        { "rowTitle": "GPU Frequency (MHz) " }
    ], [
        { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
            { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_GPU_FREQUENCY].avg" },
            { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_GPU_FREQUENCY].min" },
            { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_GPU_FREQUENCY].max" },
            { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_GPU_FREQUENCY].value" }
        ]}
    ]]
}, {
    "instance": "",
    "cells": [[
        { "rowTitle": "GPU Core Temperature\n(Celsius Degree) " }
    ], [
        { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
            { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_GPU_CORE_TEMPERATURE].avg", "fixer": "round" },
            { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_GPU_CORE_TEMPERATURE].min", "fixer": "round" },
            { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_GPU_CORE_TEMPERATURE].max", "fixer": "round" },
            { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_GPU_CORE_TEMPERATURE].value", "fixer": "round" }
        ]}
    ]]
}, {
    "instance": "",
    "cells": [[
        { "rowTitle": "GPU Memory Temperature\n(Celsius Degree) " }
    ], [
        { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
            { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_TEMPERATURE].avg", "fixer": "round" },
            { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_TEMPERATURE].min", "fixer": "round" },
            { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_TEMPERATURE].max", "fixer": "round" },
            { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_TEMPERATURE].value", "fixer": "round" }
        ]}
    ]]
}, {
    "instance": "",
    "cells": [[
        { "rowTitle": "GPU Memory Read (kB/s) " }
    ], [
        { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
            { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_READ_THROUGHPUT].avg" },
            { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_READ_THROUGHPUT].min" },
            { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_READ_THROUGHPUT].max" },
            { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_READ_THROUGHPUT].value" }
        ]}
    ]]
}, {
    "instance": "",
    "cells": [[
        { "rowTitle": "GPU Memory Write (kB/s) " }
    ], [
        { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
            { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_WRITE_THROUGHPUT].avg" },
            { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_WRITE_THROUGHPUT].min" },
            { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_WRITE_THROUGHPUT].max" },
            { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_WRITE_THROUGHPUT].value" }
        ]}
    ]]
}, {
    "instance": "",
    "cells": [[
        { "rowTitle": "GPU Memory Bandwidth (%) " }
    ], [
        { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
            { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_BANDWIDTH].avg" },
            { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_BANDWIDTH].min" },
            { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_BANDWIDTH].max" },
            { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_BANDWIDTH].value" }
        ]}
    ]]
}, {
    "instance": "",
    "cells": [[
        { "rowTitle": "GPU Memory Used (MiB) " }
    ], [
        { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
            { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_USED].avg", "scale": 1048576, "fixer": "round" },
            { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_USED].min", "scale": 1048576, "fixer": "round" },
            { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_USED].max", "scale": 1048576, "fixer": "round" },
            { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_USED].value", "scale": 1048576, "fixer": "round" }
        ]}
    ]]
}, {
    "instance": "",
    "cells": [[
        { "rowTitle": "PCIe Read (kB/s) " }
    ], [
        { "value": "", "subs": [
            { "label": "avg", "value": "device_level[metrics_type==XPUM_STATS_PCIE_READ_THROUGHPUT].avg" },
            { "label": "min", "value": "device_level[metrics_type==XPUM_STATS_PCIE_READ_THROUGHPUT].min" },
            { "label": "max", "value": "device_level[metrics_type==XPUM_STATS_PCIE_READ_THROUGHPUT].max" },
            { "label": "current", "value": "device_level[metrics_type==XPUM_STATS_PCIE_READ_THROUGHPUT].value" }
        ]}
    ]]
}, {
    "instance": "",
    "cells": [[
        { "rowTitle": "PCIe Write (kB/s) " }
    ], [
        { "value": "", "subs": [
            { "label": "avg", "value": "device_level[metrics_type==XPUM_STATS_PCIE_WRITE_THROUGHPUT].avg" },
            { "label": "min", "value": "device_level[metrics_type==XPUM_STATS_PCIE_WRITE_THROUGHPUT].min" },
            { "label": "max", "value": "device_level[metrics_type==XPUM_STATS_PCIE_WRITE_THROUGHPUT].max" },
            { "label": "current", "value": "device_level[metrics_type==XPUM_STATS_PCIE_WRITE_THROUGHPUT].value" }
        ]}
    ]]
}]
})"_json);

void ComletStatistics::setupOptions() {
    this->opts = std::unique_ptr<ComletStatisticsOptions>(new ComletStatisticsOptions());
    addOption("-d,--device", this->opts->deviceId, "The device id to query");
}

std::unique_ptr<nlohmann::json> ComletStatistics::run() {
    if (isDeviceOp()) {
        auto json = this->coreStub->getStatistics(this->opts->deviceId, true);
        return json;
    }
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    (*json)["error"] = "Unknow operation";
    return json;
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

    CharTable table(ComletConfigDeviceStatistics, json, cont);
    table.show(out);
}

void ComletStatistics::getTableResult(std::ostream& out) {
    auto res = run();
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;
    if (this->opts->groupId != 0) {
        auto devices = (*json)["datas"].get<std::vector<nlohmann::json>>();
        bool cont = false;
        for (auto device : devices) {
            showDeviceStatistics(out, std::make_shared<nlohmann::json>(device), cont);
            cont = true;
        }
    }
    else {
        showDeviceStatistics(out, json);
    }
}
