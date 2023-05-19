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
#include "local_functions.h"
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
    auto lmemOpt = addOption("--lmem", this->opts->lmemPerVf, "The memory size of each virtual GPU, such as 500M");
    lmemOpt->check([](const std::string &str) {
        std::string errStr = "Invalid lmem format";
        return std::regex_match(str, std::regex("[0-9]+M[B]{0,1}")) ? "" : "Invalid lmem format";
    });
    auto listFlag = addFlag("-l,--list", this->opts->list, "List all virtual GPUs on the specified phytsical GPU");
    auto removeFlag = addFlag("-r,--remove", this->opts->remove, "Remove all virtual GPUs on the specified physical GPU");
    addFlag("-y, --assumeyes", opts->assumeYes, "Assume that the answer to any question which would be asked is yes");

    /*
     *  All the flags should be exclusive to each other.
     */
    precheckFlag->excludes(deviceIdOpt);
    precheckFlag->excludes(numVfsOpt);
    precheckFlag->excludes(lmemOpt);
    createFlag->excludes(precheckFlag);
    createFlag->needs(deviceIdOpt);
    createFlag->needs(numVfsOpt);
    createFlag->needs(lmemOpt);
    listFlag->excludes(precheckFlag);
    listFlag->excludes(createFlag);
    listFlag->excludes(numVfsOpt);
    listFlag->excludes(lmemOpt);
    listFlag->needs(deviceIdOpt);
    removeFlag->excludes(precheckFlag);
    removeFlag->excludes(createFlag);
    removeFlag->excludes(listFlag);
    removeFlag->excludes(numVfsOpt);
    removeFlag->excludes(lmemOpt);
    removeFlag->needs(deviceIdOpt);
}

std::unique_ptr<nlohmann::json> ComletVgpu::run() {
    std::unique_ptr<nlohmann::json> json;
    int targetId = -1;
    if (isValidDeviceId(this->opts->deviceId)) {
        targetId = std::stoi(this->opts->deviceId);
    } else if (isBDF(this->opts->deviceId)) {
        auto getIdJson = this->coreStub->getDeivceIdByBDF(this->opts->deviceId.c_str(), &targetId);
        if (getIdJson->contains("error")) {
            return getIdJson;
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
            json = this->coreStub->createVf(targetId, this->opts->numVfs, lmemMb * 1024 * 1024);
            if (json->contains("error")) {
                return json;
            }
            json = this->coreStub->getDeviceFunction(targetId);
        } else {
            this->precheckPassFlag = false;
            return json;
        }
    } else if (this->opts->list) {
        json = this->coreStub->getDeviceFunction(targetId);
        return json;
    } else if (this->opts->remove) {
        json = this->coreStub->removeAllVf(targetId);
        return json;
    }
    return json;
}

void ComletVgpu::getTableResult(std::ostream &out) {
    /*
     *  Warning message for vgpu remove
     */
    if (this->opts->remove) {
        std::cout << "CAUTION: we are removing all VFs on device "
            << this->opts->deviceId
            << ", please make sure all VF-assigned virtual machines are shut down."
            << std::endl;
        std::cout << "Please confirm to proceed (y/n) ";
        if (!opts->assumeYes) {
            std::string confirm;
            std::cin >> confirm;
            if (confirm != "Y" && confirm != "y") {
                out << "Remove VFs aborted" << std::endl;
                return;
            }
        } else {
            out << std::endl;
        }
    }

    auto res = run();

    /*
     *  Get sriov_drivers_autoprobe by device ID or BDF address
     */
    std::string bdfAddress;
    if (isValidDeviceId(this->opts->deviceId)) {
        auto devicePropJson = this->coreStub->getDeviceProperties(std::stoi(this->opts->deviceId));
        if (devicePropJson->contains("error")) {
            out << "Error: " << (*devicePropJson)["error"].get<std::string>() << std::endl;
            setExitCodeByJson(*devicePropJson);
            return;
        }
        if (devicePropJson->contains("pci_bdf_address")) {
            bdfAddress = (*devicePropJson)["pci_bdf_address"].get<std::string>();
        }
    } else if (isBDF(this->opts->deviceId)) {
        bdfAddress = this->opts->deviceId;
    }
    bool isAutoprobeEnabled = isDriversAutoprobeEnabled(bdfAddress);

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

        /*
         *  Different table at autoprobe enabled/disabled
         */
        CharTable table(isAutoprobeEnabled ? functionListTableConfig : functionListTableWithoutIdConfig, *res);
        table.show(out);
    } else if (this->opts->list) {
        CharTable table(isAutoprobeEnabled ? functionListTableConfig : functionListTableWithoutIdConfig, *res);
        table.show(out);
    } else if (this->opts->remove) {
        out << "All virtual GPUs on the device " << this->opts->deviceId << " are removed." << std::endl;
    }
}

} // end namespace xpum::cli
