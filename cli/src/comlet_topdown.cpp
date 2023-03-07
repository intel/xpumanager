/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_topology.cpp
 */

#include "comlet_topdown.h"

#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <nlohmann/json.hpp>

#include "cli_table.h"
#include "core_stub.h"
#include "utility.h"
#include "exit_code.h"

using namespace std;

namespace xpum::cli {

static CharTableConfig ComletConfigTopdownDevice(R"({
    "columns": [{
        "title": "Device ID/Tile ID"
    }, {
        "title": "Top-down Detail"
    }],
    "rows": [{
        "instance": "tile_json_list[]",
        "cells": [
            "tile_id", [
                { "label": "EU in Use (%)                        ", "value": "in_use"  },
                { "label": "  EU Active (%)                      ", "value": "active"  },
                { "label": "    ALU Active (%)                   ", "value": "alu_active"  },
                { "label": "      XMX Active (%)                 ", "value": "xmx_active"  },
                { "label": "        XMX Only (%)                 ", "value": "xmx_only"  },
                { "label": "        Also w/ FPU (%)              ", "value": "xmx_fpu_active"  },
                { "label": "      FPU w/o XMX (%)                ", "value": "fpu_without_xmx"  },
                { "label": "        FPU Only (%)                 ", "value": "fpu_only"  },
                { "label": "        Also w/ EM/INT (%)           ", "value": "em_fpu_active"  },
                { "label": "      EM/INT Only (%)                ", "value": "em_int_only"  },
                { "label": "    Other Instructions (%)           ", "value": "other"  },
                { "label": "  EU Stall (%)                       ", "value": "stall"  },
                { "label": "    Low occupancy (%)                ", "value": "non_occupancy"  },
                { "label": "    ALU Dep (%)                      ", "value": "stall_alu"  },
                { "label": "    Barrier (%)                      ", "value": "stall_barrier"  },
                { "label": "    Dependency/SFU/SBID (%)          ", "value": "stall_dep"  },
                { "label": "    Other(Flag/EoT) (%)              ", "value": "stall_other"  },
                { "label": "    Instruction Fetch (%)            ", "value": "stall_inst_fetch"  },
                { "label": "EU Not in Use (%)                    ", "value": "not_in_use" },
                { "label": "  Workload Parallelism (%)           ", "value": "workload" },
                { "label": "  Engine Inefficiency (%)            ", "value": "engine" }
            ]
        ]
    }]
})"_json);

ComletTopdown::ComletTopdown() : ComletBase("topdown", "Expected feature.") {
}

void ComletTopdown::setupOptions() {
    this->opts = std::unique_ptr<ComletTopdownOptions>(new ComletTopdownOptions());
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceId, "The device ID or PCI BDF address");
    deviceIdOpt->check([](const std::string &str) {
        std::string errStr = "Device id should be a non-negative integer or a BDF string";
        if (isValidDeviceId(str)) {
            return std::string();
        } else if (isBDF(str)) {
            return std::string();
        }
        return errStr;
    });
    addOption("-t,--tile", this->opts->deviceTileId, "The device tile ID to query. If the device has only one tile, this parameter should not be specified.");
    addOption("-s,--samplingInterval", this->opts->samplingInterval, "Set the time interval (in milliseconds) by which XPU Manager daemon monitors gpu component utilization statistics.");
}

std::unique_ptr<nlohmann::json> ComletTopdown::run() {
    auto result = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    if (isDeviceOperation()) {
        int targetId = -1;
        if (isNumber(this->opts->deviceId)) {
            targetId = std::stoi(this->opts->deviceId);
        } else {
            auto convertResult = this->coreStub->getDeivceIdByBDF(this->opts->deviceId.c_str(), &targetId);
            if (convertResult->contains("error")) {
                return convertResult;
            }
        }
        auto json = this->coreStub->getDeviceComponentOccupancyRatio(targetId, this->opts->deviceTileId, this->opts->samplingInterval);
        return json;
    } else {
        (*result)["error"] = "Wrong argument or unknow operation, run with --help for more information.";
        exit_code = XPUM_CLI_ERROR_BAD_ARGUMENT;
    }

    return result;
}

static void showTopdownAnalysisResult(std::ostream &out, std::shared_ptr<nlohmann::json> json, const bool cont = false) {
    CharTable table(ComletConfigTopdownDevice, *json, cont);
    table.show(out);
}

void ComletTopdown::getTableResult(std::ostream &out) {
    auto res = run();

    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        setExitCodeByJson(*res);
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;
    
    showTopdownAnalysisResult(out, json);
}

} // end namespace xpum::cli
