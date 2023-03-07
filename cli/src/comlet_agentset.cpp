/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_agentset.cpp
 */

#include "comlet_agentset.h"

#include <nlohmann/json.hpp>

#include "cli_table.h"
#include "core_stub.h"

namespace xpum::cli {

static CharTableConfig ComletConfigAgentSetting(R"({
    "columns": [{
        "title": "Name"
    }, {
        "title": "Value"
    }],
    "rows": [{
        "instance": "",
        "cells": [
            { "rowTitle": "Sampling Interval (ms) " },
            "sampling_interval"
        ]
    }]
})"_json);

ComletAgentSet::ComletAgentSet() : ComletBase("agentset", "Get or change some XPU Manager settings.") {
    printHelpWhenNoArgs = true;
}

void ComletAgentSet::setupOptions() {
    this->opts = std::unique_ptr<ComletAgentSetOptions>(new ComletAgentSetOptions());

    auto listOpt = addFlag("-l,--list", this->opts->list, "Display all agent settings");
    auto samplingIntervalOpt = addOption("-t,--time", this->opts->samplingInterval, "Set the time interval (in milliseconds) by which XPU Manager daemon retrieve raw gpu statistics. Valid values include 100,200,500,1000.");
    samplingIntervalOpt->check(CLI::IsMember({100, 200, 500, 1000}));

    listOpt->excludes(samplingIntervalOpt);
}

std::unique_ptr<nlohmann::json> ComletAgentSet::run() {
    std::unique_ptr<nlohmann::json> json;
    if (this->opts->list) {
        return this->coreStub->getAgentConfig();
    } else if (this->opts->samplingInterval != -1) {
        int64_t sampling_interval = this->opts->samplingInterval;
        return this->coreStub->setAgentConfig("sampling_interval", &sampling_interval);
    } else {
        auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
        (*json)["error"] = "Unknow operation";
        return json;
    }
}

static void showResult(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    CharTable table(ComletConfigAgentSetting, *json);
    table.show(out);
}

void ComletAgentSet::getTableResult(std::ostream &out) {
    auto res = run();
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        setExitCodeByJson(*res);
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;

    showResult(out, json);
}

} // namespace xpum::cli
