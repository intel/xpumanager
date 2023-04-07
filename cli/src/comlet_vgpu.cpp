/* 
 *  Copyright (C) 2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_vgpu.cpp
 */

#include "comlet_vgpu.h"

#include <nlohmann/json.hpp>

#include "core_stub.h"
#include "cli_table.h"
#include "utility.h"
#include "exit_code.h"
#include <iostream>

namespace xpum::cli {

static nlohmann::json precheckTable = R"({
    "showTitleRow": false,
    "columns": [{
        "title": "none"
    }, {
        "title": "none"
    }],
    "rows": [{
        "instance": "",
        "cells": [
            {"rowTitle": "VMX Flag"},
            [{ "label": "Result", "value": "vmx_flag" }, {"label": "Message", "value": "vmx_message"}]
        ]
    },{
        "instance": "",
        "cells": [
            {"rowTitle": "SR-IOV"},
            [{ "label": "Result", "value": "sriov_status" }, {"label": "Message", "value": "sriov_message"}]
        ]
    },{
        "instance": "",
        "cells": [
            {"rowTitle": "IOMMU"},
            [{ "label": "Result", "value": "iommu_status" }, {"label": "Message", "value": "iommu_message"}]
        ]
    }]
})"_json;

static CharTableConfig precheckTableConfig(precheckTable);

ComletVgpu::ComletVgpu() : ComletBase("vgpu", 
    "Create and remove virtual GPUs in SRIOV configuration.") {
    printHelpWhenNoArgs = true;
}

void ComletVgpu::setupOptions() {
    this->opts = std::unique_ptr<ComletVgpuOptions>(new ComletVgpuOptions());
    addFlag("--precheck", this->opts->precheck, "Check if BIOS settings are ready to create virtual GPUs");
}

std::unique_ptr<nlohmann::json> ComletVgpu::run() {
    std::unique_ptr<nlohmann::json> json;
    if (this->opts->precheck) {
        json = this->coreStub->doVgpuPrecheck();
    }
    return json;
}

void ComletVgpu::getTableResult(std::ostream &out) {
    auto res = run();
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        setExitCodeByJson(*res);
        return;
    }
    CharTable table(precheckTableConfig, *res);
    table.show(out);
}

} // end namespace xpum::cli
