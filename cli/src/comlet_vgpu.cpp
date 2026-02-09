/* 
 *  Copyright (C) 2023-2024 Intel Corporation
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

static nlohmann::json statsTable = R"({
    "columns": [{
        "title": "Device Information"
    }],
    "rows": [{
        "instance": "vf_list[]",
        "cells": [
            [
                { "label": "PCI BDF Address", "value": "bdf_address" },
                { "label": "Average % utilization of all GPU Engines ", "value": "gpu_util", "fixer": "roundtwodecimals" },
                { "label": "Compute Engine Util(%) ", "value": "ce_util", "fixer": "roundtwodecimals" },
                { "label": "Render Engine Util (%) ", "value": "re_util", "fixer": "roundtwodecimals" },
                { "label": "Media Engine Util (%) ", "value": "me_util", "fixer": "roundtwodecimals" },
                { "label": "Copy Engine Util (%) ", "value": "coe_util", "fixer": "roundtwodecimals" },
                { "label": "GPU Memory Util (%) ", "value": "mem_util", "fixer": "roundtwodecimals" }
            ]
        ]
    }]
})"_json;

static CharTableConfig precheckTableConfig(precheckTable);
static CharTableConfig functionListTableConfig(functionListTable);
static CharTableConfig functionListTableWithoutIdConfig(functionListTableWithoutId);
static CharTableConfig statsTableConfig(statsTable);

ComletVgpu::ComletVgpu() : ComletBase("vgpu", 
    "Create and remove virtual GPUs in SRIOV configuration.") {
    printHelpWhenNoArgs = true;
}

void ComletVgpu::setupOptions() {
    this->opts = std::unique_ptr<ComletVgpuOptions>(new ComletVgpuOptions());
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
    auto kernFlag = addFlag("--addkernelparam", this->opts->kern, "Add the kernel command line parameters for the virtual GPUs");
    auto precheckFlag = addFlag("--precheck", this->opts->precheck, "Check if BIOS settings are ready to create virtual GPUs");
    auto createFlag = addFlag("-c,--create", this->opts->create, "Create the virtual GPUs");
    auto numVfsOpt = addOption("-n", this->opts->numVfs, "The number of virtual GPUs to create");
    auto lmemOpt = addOption("--lmem", this->opts->lmemPerVf, "The memory size of each virtual GPUs, in MiB. For example, --lmem 500.");
    lmemOpt->check([](const std::string &str) {
        return std::regex_match(str, std::regex("^[0-9]*[1-9]+[0-9]*[M]{0,1}$")) ? "" : "Invalid lmem format";
    });
    auto removeFlag = addFlag("-r,--remove", this->opts->remove, "Remove all virtual GPUs on the specified physical GPU");
    auto listFlag = addFlag("-l,--list", this->opts->list, "List all virtual GPUs on the specified phytsical GPU");
    addFlag("-y, --assumeyes", opts->assumeYes, "Assume that the answer to any question which would be asked is yes");
    auto statsFlag = addFlag("-s, --stats", this->opts->stats, "Show statistics data of all virtual GPUs");

    /*
     *  All the flags should be exclusive to each other.
     */
    precheckFlag->needs(deviceIdOpt);
    precheckFlag->excludes(numVfsOpt);
    precheckFlag->excludes(lmemOpt);
    createFlag->excludes(precheckFlag);
    createFlag->needs(deviceIdOpt);
    createFlag->needs(numVfsOpt);
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
    kernFlag->excludes(precheckFlag);
    kernFlag->excludes(createFlag);
    kernFlag->excludes(listFlag);
    kernFlag->excludes(removeFlag);
    kernFlag->excludes(deviceIdOpt);
    kernFlag->excludes(numVfsOpt);
    kernFlag->excludes(lmemOpt);
    statsFlag->excludes(precheckFlag);
    statsFlag->excludes(createFlag);
    statsFlag->excludes(listFlag);
    statsFlag->excludes(numVfsOpt);
    statsFlag->excludes(lmemOpt);
    statsFlag->excludes(removeFlag);
    statsFlag->excludes(kernFlag);
    statsFlag->needs(deviceIdOpt);
}

std::unique_ptr<nlohmann::json> ComletVgpu::run() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    int targetId = -1;
    if (isValidDeviceId(this->opts->deviceId)) {
        targetId = std::stoi(this->opts->deviceId);
    } else if (isBDF(this->opts->deviceId)) {
        auto getIdJson = this->coreStub->getDeivceIdByBDF(this->opts->deviceId.c_str(), &targetId);
        if (getIdJson->contains("error")) {
            return getIdJson;
        }
    }

    /*
     *  Do precheck first, if failed, stop creating/listing/removing VFs.
     */
    if (this->opts->create || this->opts->list || this->opts->remove || 
        this->opts->stats) {
        json = this->coreStub->doVgpuPrecheck(targetId);
        if (json->contains("iommu_status") && (*json)["iommu_status"].get<std::string>().compare("Pass") == 0
            && json->contains("sriov_status") && (*json)["sriov_status"].get<std::string>().compare("Pass") == 0
            && json->contains("vmx_flag") && (*json)["vmx_flag"].get<std::string>().compare("Pass") == 0
        ) {
            this->precheckPassFlag = true;
        } else {
            this->precheckPassFlag = false;
            return json;
        }
    }

    if (this->opts->precheck) {
        json = this->coreStub->doVgpuPrecheck(targetId);
        if (json->contains("vmx_flag") && (*json)["vmx_flag"].get<std::string>().compare("Fail") == 0) {
            (*json)["errno"] = XPUM_CLI_ERROR_VGPU_NO_VMX_FLAG;
        } else if (json->contains("iommu_status") && (*json)["iommu_status"].get<std::string>().compare("Fail") == 0) {
            (*json)["errno"] = XPUM_CLI_ERROR_VGPU_IOMMU_DISABLED;
        } else if (json->contains("sriov_status") && (*json)["sriov_status"].get<std::string>().compare("Fail") == 0) {
            (*json)["errno"] = XPUM_CLI_ERROR_VGPU_SRIOV_DISABLED;
        }
    } else if (this->opts->create) {
        uint64_t lmemMb = 0;
        if (this->opts->lmemPerVf.size()) {
            try {
                lmemMb = std::stoul(std::regex_replace(this->opts->lmemPerVf, std::regex("[^0-9]"), ""));
            } catch (std::exception &e) {
                (*json)["error"] = "Bad lmem argument";
                (*json)["errno"] = XPUM_CLI_ERROR_BAD_ARGUMENT;
                return json;
            }
        }
        json = this->coreStub->createVf(targetId, this->opts->numVfs, lmemMb * 1024 * 1024);
        if (json->contains("error")) {
            return json;
        }
        json = this->coreStub->getDeviceFunction(targetId);
    } else if (this->opts->list) {
        json = this->coreStub->getDeviceFunction(targetId);
    } else if (this->opts->remove) {
        json = this->coreStub->removeAllVf(targetId);
    } else if (this->opts->kern) {
        json = addKernelParam();
    } else if (this->opts->stats) {
        json = this->coreStub->getVfMetrics(targetId);
    } else {
        (*json)["error"] = "Wrong argument or unknown operation, run with --help for more information.";
        (*json)["errno"] = XPUM_CLI_ERROR_BAD_ARGUMENT;
    }
    return json;
}

void ComletVgpu::getTableResult(std::ostream &out) {
    /*
     *  Warning message for vgpu remove and addkernelparam
     */
    if (this->opts->remove) {
        std::cout << "Do you want to remove all virtual GPUs? (y/n)";
        if (!opts->assumeYes) {
            std::string confirm;
            std::cin >> confirm;
            if (confirm != "Y" && confirm != "y") {
                out << "Remove virtual GPUs aborted" << std::endl;
                return;
            }
        } else {
            out << std::endl;
        }
    } else if (this->opts->kern) {
        std::cout << "Do you want to add the required kernel command line parameters? (y/n) ";
        if (!opts->assumeYes) {
            std::string confirm;
            std::cin >> confirm;
            if (confirm != "Y" && confirm != "y") {
                out << "Add kernel parameters aborted" << std::endl;
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
        setExitCodeByJson(*res);
    } else if (this->opts->create || this->opts->list) {
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
    } else if (this->opts->remove) {
        if (!this->precheckPassFlag) {
            CharTable table(precheckTableConfig, *res);
            table.show(out);
            return;
        }
        out << "All virtual GPUs on the device " << this->opts->deviceId << " are removed." << std::endl;
    } else if (this->opts->kern) {
        if (isATSMPlatformFromSysFile())
            out << "Succeed to add the required kernel command line parameters, \"intel_iommu=on i915.max_vfs=31\". \"intel_iommmu\" is for IOMMU and \"i915.max_vfs\" is for SR-IOV. Please reboot OS to take effect." << std::endl;
        else
            out << "Succeed to add the required kernel command line parameters, \"intel_iommu=on iommu=pt i915.force_probe=* i915.max_vfs=63 i915.enable_iaf=0\". Please reboot OS to take effect." << std::endl;
    } else if (this->opts->stats == true) {
        CharTable table(statsTableConfig, *res);
        table.show(out);
        return;
    }
}

bool ComletVgpu::isAddKernelParam() {
    return this->opts->kern;
}

} // end namespace xpum::cli
