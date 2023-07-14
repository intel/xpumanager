/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_discovery.cpp
 */

#include "comlet_discovery.h"

#include <regex>
#include <map>
#include <stack>
#include <nlohmann/json.hpp>

#include "cli_table.h"
#include "core_stub.h"
#include "exit_code.h"
#include "utility.h"

namespace xpum::cli {

    static nlohmann::json discoveryBasicJson = R"({
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
})"_json;


    // To add a new data to dump, the dumpId should be max( 
    // dumpId in discoveryDetailedJson and function initDumpPropConfig) + 1
    static nlohmann::json discoveryDetailedJson = R"({
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
                { "label": "Device Name", "value": "device_name", "dumpId": 2 },
                { "label": "PCI Device ID", "value": "pci_device_id", "dumpId": 24},
                { "label": "Vendor Name", "value": "vendor_name", "dumpId": 3 },
                { "label": "UUID", "value": "uuid", "dumpId": 4 },
                { "label": "Serial Number", "value": "serial_number", "dumpId": 5 },
                { "label": "Core Clock Rate", "value": "core_clock_rate_mhz", "suffix": " MHz", "dumpId": 6 },
                { "label": "Stepping", "value": "device_stepping", "dumpId": 7 },
                { "label": "SKU Type", "value": "sku_type"},
                { "rowTitle": " " },
                { "label": "Driver Version", "value": "driver_version", "dumpId": 8 },
                { "label": "Kernel Version", "value": "kernel_version" },
                { "label": "GFX Firmware Name", "value": "gfx_firmware_name" },
                { "label": "GFX Firmware Version", "value": "gfx_firmware_version", "dumpId": 9 },
                { "label": "GFX Firmware Status", "value": "gfx_firmware_status", "dumpId": 22 },
                { "label": "GFX Data Firmware Name", "value": "gfx_data_firmware_name" },
                { "label": "GFX Data Firmware Version", "value": "gfx_data_firmware_version", "dumpId": 10 },
                { "label": "GFX PSC Firmware Name", "value": "gfx_pscbin_firmware_name" },
                { "label": "GFX PSC Firmware Version", "value": "gfx_pscbin_firmware_version"},)"
                R"({ "label": "AMC Firmware Name", "value": "amc_firmware_name"},)"
                R"({ "label": "AMC Firmware Version", "value": "amc_firmware_version"},)"
                R"({ "rowTitle": " " },
                { "label": "PCI BDF Address", "value": "pci_bdf_address", "dumpId": 11 },
                { "label": "PCI Slot", "value": "pci_slot", "dumpId": 12 },
                { "label": "PCIe Generation", "value": "pcie_generation", "dumpId": 13 },
                { "label": "PCIe Max Link Width", "value": "pcie_max_link_width", "dumpId": 14 },
                { "label": "OAM Socket ID", "value": "oam_socket_id", "dumpId": 15 },
                { "rowTitle": " " },
                { "label": "Memory Physical Size", "value": "memory_physical_size_byte", "suffix": " MiB", "scale": 1048576, "dumpId": 16 },
                { "label": "Max Mem Alloc Size", "value": "max_mem_alloc_size_byte", "suffix": " MiB", "scale": 1048576 },
                { "label": "ECC State", "value": "memory_ecc_state" },
                { "label": "Number of Memory Channels", "value": "number_of_memory_channels", "dumpId": 17 },
                { "label": "Memory Bus Width", "value": "memory_bus_width", "dumpId": 18 },
                { "label": "Max Hardware Contexts", "value": "max_hardware_contexts" },
                { "label": "Max Command Queue Priority", "value": "max_command_queue_priority" },
                { "rowTitle": " " },
                { "label": "Number of EUs", "value": "number_of_eus", "dumpId": 19 },
                { "label": "Number of Tiles", "value": "number_of_tiles" },
                { "label": "Number of Slices", "value": "number_of_slices" },
                { "label": "Number of Sub Slices per Slice", "value": "number_of_sub_slices_per_slice" },
                { "label": "Number of Threads per EU", "value": "number_of_threads_per_eu" },
                { "label": "Physical EU SIMD Width", "value": "physical_eu_simd_width" },
                { "label": "Number of Media Engines", "value": "number_of_media_engines", "dumpId": 20 },
                { "label": "Number of Media Enhancement Engines", "value": "number_of_media_enh_engines", "dumpId": 21 },
                { "rowTitle": " " },
                { "label": "Number of Xe Link ports", "value": "number_of_fabric_ports" },
                { "label": "Max Tx/Rx Speed per Xe Link port", "value": "max_fabric_port_speed", "suffix": " MiB/s", "scale": 1 },
                { "label": "Number of Lanes per Xe Link port", "value": "number_of_lanes_per_fabric_port" }
            ]
        ]
    }]
})"_json;

    ComletDiscovery::ComletDiscovery() : ComletBase("discovery", "Discover the GPU devices installed on this machine and provide the device info.") {
    }

    static CharTableConfig ComletConfigDiscoveryBasic(discoveryBasicJson);
    static CharTableConfig ComletConfigDiscoveryDetailed(discoveryDetailedJson);

    void ComletDiscovery::setupOptions() {
        this->opts = std::unique_ptr<ComletDiscoveryOptions>(new ComletDiscoveryOptions());
        auto deviceIdOpt = addOption("-d,--device", this->opts->deviceId, "Device ID or PCI BDF address to query. It will show more detailed info.");
        deviceIdOpt->check([](const std::string& str) {
            std::string errStr = "Device id should be a non-negative integer or a BDF string";
            if (isValidDeviceId(str)) {
                return std::string();
            }
            else if (isBDF(str)) {
                return std::string();
            }
            return errStr;
            });
        auto listamcversionsOpt = addFlag("--listamcversions", this->opts->listamcversions, "Show all AMC firmware versions.");
        deviceIdOpt->excludes(listamcversionsOpt);
    }

    std::unique_ptr<nlohmann::json> ComletDiscovery::run() {
        if (this->opts->listamcversions) {
            auto json = this->coreStub->getAMCFirmwareVersions(this->opts->username, this->opts->password);
            return json;
        }

        if (this->opts->deviceId.compare("-1") != 0) {
            auto json = std::make_unique<nlohmann::json>();
            if (isNumber(this->opts->deviceId)) {
                json = this->coreStub->getDeviceProperties(std::stoi(this->opts->deviceId), this->opts->username, this->opts->password);
            }
            else {
                json = this->coreStub->getDeviceProperties(this->opts->deviceId.c_str(), this->opts->username, this->opts->password);
            }
            if (json->contains("serial_number") && (*json)["serial_number"].get<std::string>().compare("unknown") == 0) {
                std::string sn = this->coreStub->getSerailNumberIPMI(std::stoi(this->opts->deviceId));
                if (sn.size() > 0) {
                    (*json)["serial_number"] = sn;
                }
            }
            return json;
        }

        auto json = this->coreStub->getDeviceList();
        return json;
    }

    static void showBasicInfo(std::ostream& out, std::shared_ptr<nlohmann::json> json) {
        if (!json->contains("device_list") || (*json)["device_list"].size() <= 0) {
            out << "No device discovered" << std::endl;
            return;
        }

        CharTable table(ComletConfigDiscoveryBasic, *json);
        table.show(out);
    }

    static void showDetailedInfo(std::ostream& out, std::shared_ptr<nlohmann::json> json) {
        nlohmann::json js = *json;
        std::string ver = js["pci_device_id"];
        if (ver.length() != 0 && isATSMPlatform(ver)) {
            js["serial_number"] = XPUM_TABLE_HIDE_TAG;
            js["device_stepping"] = XPUM_TABLE_HIDE_TAG;
            js["sku_type"] = XPUM_TABLE_HIDE_TAG;
            js["pci_slot"] = XPUM_TABLE_HIDE_TAG;
            js["oam_socket_id"] = XPUM_TABLE_HIDE_TAG;
            js["max_command_queue_priority"] = XPUM_TABLE_HIDE_TAG;
            js["number_of_fabric_ports"] = XPUM_TABLE_HIDE_TAG;
            js["max_fabric_port_speed"] = XPUM_TABLE_HIDE_TAG;
            js["number_of_lanes_per_fabric_port"] = XPUM_TABLE_HIDE_TAG;
            js["kernel_version"] = XPUM_TABLE_HIDE_TAG;
            js["gfx_firmware_status"] = XPUM_TABLE_HIDE_TAG;
            js["gfx_pscbin_firmware_name"] = XPUM_TABLE_HIDE_TAG;
            js["gfx_pscbin_firmware_version"] = XPUM_TABLE_HIDE_TAG;
            js["amc_firmware_name"] = XPUM_TABLE_HIDE_TAG;
            js["amc_firmware_version"] = XPUM_TABLE_HIDE_TAG;
        }
        CharTable table(ComletConfigDiscoveryDetailed, js);
        table.show(out);
    }

    static void showAmcFwVersion(std::ostream& out, std::shared_ptr<nlohmann::json> json) {
        auto versions = (*json)["amc_fw_version"];
        out << versions.size() << " AMC are found" << std::endl;
        int i = 0;
        for (auto version : versions) {
            out << "AMC " << i++ << " firmware version: " << version.get<std::string>() << std::endl;
        }
    }

    void ComletDiscovery::getTableResult(std::ostream& out) {
        if (this->opts->listamcversions || this->opts->deviceId.compare("-1") != 0) {
            // show warning message
            std::string amcWarnMsg = coreStub->getRedfishAmcWarnMsg();
            if (amcWarnMsg.length()) {
                out << coreStub->getRedfishAmcWarnMsg() << std::endl;
                out << "Do you want to continue? (y/n)" << std::endl;
                std::string confirm;
                std::cin >> confirm;
                if (confirm != "Y" && confirm != "y") {
                    out << "Aborted" << std::endl;
                    return;
                }
            }
        }
        auto res = run();
        if (res->contains("error")) {
            out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
            setExitCodeByJson(*res);
            return;
        }
        std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
        *json = *res;

        if (this->opts->listamcversions) {
            showAmcFwVersion(out, json);
        }
        else if (this->opts->deviceId.compare("-1") != 0) {
            showDetailedInfo(out, json);
            if (_stricmp(std::string((*json)["gfx_firmware_version"]).c_str(), "unknown") == 0 ||
                _stricmp(std::string((*json)["gfx_data_firmware_version"]).c_str(), "unknown") == 0) {
                exit_code = XPUM_CLI_ERROR_FIRMWARE_VERSION_ERROR;
            }
        }
        else {
            showBasicInfo(out, json);
        }
    }
} // end namespace xpum::cli
