/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_discovery.cpp
 */

#include "comlet_discovery.h"

#include <map>
#include <nlohmann/json.hpp>

#include "cli_table.h"
#include "core_stub.h"

namespace xpum::cli {

static CharTableConfig ComletConfigDiscoveryBasic(R"({
    "columns": [{
        "title": "Device ID"
    }, {
        "title": "Device Information"
    }],
    "rows": [{
        "instance": "device_list[]",
        "cells": [
            "device_id", [
                { "label": "Device Name", "value": "device_name" },
                { "label": "Vendor Name", "value": "vendor_name" },
                { "label": "UUID", "value": "uuid" },
                { "label": "PCI BDF Address", "value": "pci_bdf_address" }
            ]
        ]
    }]
})"_json);

static CharTableConfig ComletConfigDiscoveryDetailed(R"({
    "columns": [{
        "title": "Device ID"
    }, {
        "title": "Device Information"
    }],
    "rows": [{
        "instance": "",
        "cells": [
            "device_id", [
                { "label": "Device Type", "value": "device_type" },
                { "label": "Device Name", "value": "device_name" },
                { "label": "Vendor Name", "value": "vendor_name" },
                { "label": "UUID", "value": "uuid" },
                { "label": "Serial Number", "value": "serial_number" },
                { "label": "Core Clock Rate", "value": "core_clock_rate_mhz", "suffix": " MHz" },
                { "rowTitle": " " },
                { "label": "Driver Version", "value": "driver_version" },
                { "label": "Firmware Name", "value": "firmware_name" },
                { "label": "Firmware Version", "value": "firmware_version" },
                { "rowTitle": " " },
                { "label": "PCI BDF Address", "value": "pci_bdf_address" },
                { "label": "PCI Slot", "value": "pci_slot" },
                { "label": "PCIe Generation", "value": "pcie_generation" },
                { "label": "PCIe Max Link Width", "value": "pcie_max_link_width" },
                { "rowTitle": " " },
                { "label": "Memory Physical Size", "value": "memory_physical_size_byte", "suffix": " MiB", "scale": 1048576 },
                { "label": "Max Mem Alloc Size", "value": "max_mem_alloc_size_byte", "suffix": " MiB", "scale": 1048576 },
                { "label": "Number of Memory Channels", "value": "number_of_memory_channels" },
                { "label": "Memory Bus Width", "value": "memory_bus_width" },
                { "label": "Max Hardware Contexts", "value": "max_hardware_contexts" },
                { "label": "Max Command Queue Priority", "value": "max_command_queue_priority" },
                { "rowTitle": " " },
                { "label": "Number of Tiles", "value": "number_of_tiles" },
                { "label": "Number of Slices", "value": "number_of_slices" },
                { "label": "Number of Sub Slices per Slice", "value": "number_of_sub_slices_per_slice" },
                { "label": "Number of EUs per Sub Slice", "value": "number_of_eus_per_sub_slice" },
                { "label": "Number of Threads per EU", "value": "number_of_threads_per_eu" },
                { "label": "Physical EU SIMD Width", "value": "physical_eu_simd_width" },
                { "rowTitle": " " },
                { "label": "Number of Xe Link ports", "value": "number_of_fabric_ports" },
                { "label": "Max Tx/Rx Speed per Xe Link port", "value": "max_fabric_port_speed", "suffix": " MiB/s", "scale": 1048576 },
                { "label": "Number of Lanes per Xe Link port", "value": "number_of_lanes_per_fabric_port" }
            ]
        ]
    }]
})"_json);

ComletDiscovery::ComletDiscovery() : ComletBase("discovery", "Discover the GPU devices installed on this machine and provide the device info.") {
}

static bool isNumber(const std::string &str) {
    return str.find_first_not_of("0123456789") == std::string::npos;
}

void ComletDiscovery::setupOptions() {
    this->opts = std::unique_ptr<ComletDiscoveryOptions>(new ComletDiscoveryOptions());
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceId, "Device ID to query. It will show more detailed info.");
    deviceIdOpt->check([](const std::string &str) {
        std::string errStr = "Device id should be integer larger than or equal to 0";
        if (!isNumber(str))
            return errStr;
        int value;
        try {
            value = std::stoi(str);
        } catch (const std::out_of_range &oor) {
            return errStr;
        }
        if (value < 0)
            return errStr;
        return std::string();
    });
    auto listamcversionsOpt = addFlag("--listamcversions", this->opts->listamcversions, "Show all AMC firmware versions.");
    deviceIdOpt->excludes(listamcversionsOpt);
}

std::unique_ptr<nlohmann::json> ComletDiscovery::run() {
    if (this->opts->listamcversions) {
        auto json = this->coreStub->getAMCFirmwareVersions();
        return json;
    }

    if (this->opts->deviceId != -1) {
        auto json = this->coreStub->getDeviceProperties(this->opts->deviceId);
        return json;
    }

    auto json = this->coreStub->getDeviceList();
    return json;
}

static void showBasicInfo(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    if (!json->contains("device_list") || (*json)["device_list"].size() <= 0) {
        out << "No device discovered" << std::endl;
        return;
    }

    CharTable table(ComletConfigDiscoveryBasic, *json);
    table.show(out);
}

static void showDetailedInfo(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    CharTable table(ComletConfigDiscoveryDetailed, *json);
    table.show(out);
}

static void showAmcFwVersion(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    auto versions = (*json)["amc_fw_version"];
    out << versions.size() << " AMC are found" << std::endl;
    int i = 1;
    for (auto version : versions) {
        out << "AMC " << i++ << " firmware version: " << version.get<std::string>() << std::endl;
    }
}

void ComletDiscovery::getTableResult(std::ostream &out) {
    auto res = run();
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;

    if (this->opts->listamcversions) {
        showAmcFwVersion(out, json);
    } else if (this->opts->deviceId != -1) {
        showDetailedInfo(out, json);
    } else {
        showBasicInfo(out, json);
    }
}
} // end namespace xpum::cli
