/* 
 *  Copyright (C) 2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_vgpu.cpp
 */

#include "comlet_vgpu.h"

#include <nlohmann/json.hpp>
#include <stdio.h>

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

static nlohmann::json functionListTable = R"({
    "columns": [{
        "title": "Device ID"
    }, {
        "title": "Device Information"
    }],
    "rows": [{
        "instance": "vf_list[]",
        "cells": [
            "device_id", [
                { "label": "PCI BDF Address", "value": "bdf_address" },
                { "label": "Function Type", "value": "function_type" },
                { "label": "Memory Physical Size", "value": "lmem_size", "suffix": " MiB", "scale": 1048576 }
            ]
        ]
    }]
})"_json;

static nlohmann::json functionListTableWithoutId = R"({
    "columns": [{
        "title": "Device Information"
    }],
    "rows": [{
        "instance": "vf_list[]",
        "cells": [
            [
                { "label": "PCI BDF Address", "value": "bdf_address" },
                { "label": "Function Type", "value": "function_type" },
                { "label": "Memory Physical Size", "value": "lmem_size", "suffix": " MiB", "scale": 1048576 }
            ]
        ]
    }]
})"_json;

static CharTableConfig precheckTableConfig(precheckTable);
static CharTableConfig functionListTableConfig(functionListTable);
static CharTableConfig functionListTableWithoutIdConfig(functionListTableWithoutId);

ComletVgpu::ComletVgpu() : ComletBase("vgpu", 
    "Create and remove virtual GPUs in SRIOV configuration.") {
    printHelpWhenNoArgs = true;
}

void ComletVgpu::setupOptions() {
    this->opts = std::unique_ptr<ComletVgpuOptions>(new ComletVgpuOptions());
    auto precheckFlag = addFlag("--precheck", this->opts->precheck, "Check if BIOS settings are ready to create virtual GPUs");
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceId, "Device ID or PCI BDF address");
    deviceIdOpt->check([](const std::string &str) {
        std::string errStr = "Device id should be a non-negative integer or a BDF string";
        if (isValidDeviceId(str)) {
            return std::string();
        } else if (isBDF(str)) {
            return std::string();
        }
        return errStr;
    });
    auto createFlag = addFlag("-c,--create", this->opts->create, "Create the virtual GPUs");
    auto numVfsOpt = addOption("-n", this->opts->numVfs, "The number of virtual GPUs to create");
    auto lmemOpt = addOption("--lmem", this->opts->lmemPerVf, "The memory size of each virtual GPUs, in MiB");
    lmemOpt->check([](const std::string &str) {
        std::string errStr = "Invalid lmem format";
        return std::regex_match(str, std::regex("[0-9]+M[B]{0,1}")) ? "" : "Invalid lmem format";
    });
    precheckFlag->excludes(deviceIdOpt);
    precheckFlag->excludes(numVfsOpt);
    precheckFlag->excludes(lmemOpt);
    createFlag->excludes(precheckFlag);
    createFlag->needs(deviceIdOpt);
    createFlag->needs(numVfsOpt);
    createFlag->needs(lmemOpt);
}

std::unique_ptr<nlohmann::json> ComletVgpu::run() {
    std::unique_ptr<nlohmann::json> json;
    if (this->opts->deviceId.size()) {
        if (isValidDeviceId(this->opts->deviceId)) {
            this->parsedDeviceId = std::stoi(this->opts->deviceId);
        } else {
            auto getIdJson = this->coreStub->getDeivceIdByBDF(this->opts->deviceId.c_str(), &this->parsedDeviceId);
            if (getIdJson->contains("error")) {
                return getIdJson;
            }
        }
    }
    if (this->opts->precheck) {
        json = this->coreStub->doVgpuPrecheck();
    } else if (this->opts->create) {
        /*
         *  Do precheck first, if failed, stop creating VFs.
         */
        json = this->coreStub->doVgpuPrecheck();
        if (json->contains("iommu_status") && (*json)["iommu_status"].get<std::string>().compare("Pass") == 0
            && json->contains("sriov_status") && (*json)["sriov_status"].get<std::string>().compare("Pass") == 0
            && json->contains("vmx_flag") && (*json)["vmx_flag"].get<std::string>().compare("Pass") == 0
        ) {
            this->precheckPassFlag = true;
            uint64_t lmemMb = 0;
            sscanf(this->opts->lmemPerVf.c_str(), "%luM", &lmemMb);
            json = this->coreStub->createVf(this->parsedDeviceId, this->opts->numVfs, lmemMb * 1024 * 1024);
            if (json->contains("error")) {
                return json;
            }
            json = this->coreStub->getDeviceFunction(stoi(this->opts->deviceId));
        } else {
            this->precheckPassFlag = false;
            return json;
        }
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
    if (this->opts->precheck) {
        CharTable table(precheckTableConfig, *res);
        table.show(out);
    } else if (this->opts->create) {
        /*
         *  If precheck failed, show precheck table
         */
        if (!this->precheckPassFlag) {
            CharTable table(precheckTableConfig, *res);
            table.show(out);
            return;
        }
        auto devicePropJson = this->coreStub->getDeviceProperties(this->parsedDeviceId);
        if (devicePropJson->contains("pci_bdf_address")) {
            std::stringstream path, content;
            path << "/sys/bus/pci/devices/" << (*devicePropJson)["pci_bdf_address"].get<std::string>() << "/sriov_drivers_autoprobe";
            std::ifstream ifs(path.str(), std::ios::in);
            content << ifs.rdbuf();
            /*
             *  Different table at autoprobe enabled/disabled
             */
            CharTable table(std::stoi(content.str()) ? functionListTableConfig : functionListTableWithoutIdConfig, *res);
            table.show(out);
        } 
    }
}

} // end namespace xpum::cli
