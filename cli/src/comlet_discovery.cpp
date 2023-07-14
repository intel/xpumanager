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
#include "utility.h"
#include "exit_code.h"
#include "local_functions.h"

namespace xpum::cli {

#define ALL_PROP_ID -1

typedef struct dump_prop_config {
    std::string label;
    std::string value;
    int dumpId;
    std::string suffix;
    double scale;
} dump_prop_config;

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
                { "label": "PCI BDF Address", "value": "pci_bdf_address" },
                { "label": "DRM Device", "value": "drm_device" },
                { "label": "Function Type", "value": "device_function_type" }
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
                { "label": "GFX Data Firmware Name", "value": "gfx_data_firmware_name", "empty": false },
                { "label": "GFX Data Firmware Version", "value": "gfx_data_firmware_version", "dumpId": 10, "empty": false},
                { "label": "GFX PSC Firmware Name", "value": "gfx_pscbin_firmware_name", "empty": false },
                { "label": "GFX PSC Firmware Version", "value": "gfx_pscbin_firmware_version", "empty": false},)"
                R"({ "label": "AMC Firmware Name", "value": "amc_firmware_name", "empty": false },)"
                R"({ "label": "AMC Firmware Version", "value": "amc_firmware_version", "empty": false },)"
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
static std::vector<dump_prop_config> dumpFieldConfig;

static void readDumpPropConfig(nlohmann::json &conf, std::stack<std::string> &keys, 
                           std::vector<dump_prop_config> &fields) {
    if (keys.empty()) {
        if (conf.is_array()) {
            for (size_t i = 0; i < conf.size(); i++) {
                auto itemDef = conf.at(i);
                if (!itemDef.contains("dumpId")) {
                    continue;
                }
                if (itemDef.contains("label") && itemDef.contains("value")) {
                    dump_prop_config prop;
                    prop.label = itemDef["label"];
                    prop.value = itemDef["value"];
                    prop.suffix = itemDef.contains("suffix") ? itemDef["suffix"] : "";

                    if (itemDef.contains("scale")) {
                        prop.scale = static_cast<double>(itemDef["scale"]);
                    } else {
                        prop.scale = 0;
                    }

                    prop.dumpId = static_cast<int>(itemDef["dumpId"]);
                    fields.push_back(prop);
                }
            } 
        }
        return;
    }

    auto items = conf.find(keys.top());
    keys.pop();
    for (size_t i = 0; i < items->size(); i++) {
        readDumpPropConfig(items->at(i), keys, fields);
    }
}

bool compareDumpConfig(dump_prop_config dpc1, dump_prop_config dpc2) {
    return dpc1.dumpId < dpc2.dumpId;
}

static void initDumpPropConfig() {
    std::stack<std::string> keys;
    
    keys.push("cells");
    keys.push("rows");

    dumpFieldConfig.clear();
    //Add "for dump only" data here
    dumpFieldConfig.push_back(dump_prop_config{"Device ID", "device_id", 1, "", 0});
    dumpFieldConfig.push_back(dump_prop_config{"PCI Vendor ID", "pci_vendor_id", 23, "", 0});

    readDumpPropConfig(discoveryDetailedJson, keys, dumpFieldConfig);
    std::sort(dumpFieldConfig.begin(), dumpFieldConfig.end(), 
            compareDumpConfig);
}

static std::unique_ptr<dump_prop_config> getDumpPropConfig(int dumpId) {
    for (auto& conf : dumpFieldConfig) {
        if (conf.dumpId == dumpId) {
            auto prop = std::make_unique<dump_prop_config>();
            *prop = conf;
            return prop;
        }
    }
    return nullptr;
}

void ComletDiscovery::setupOptions() {
    initDumpPropConfig();
    this->opts = std::unique_ptr<ComletDiscoveryOptions>(new ComletDiscoveryOptions());
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceId, "Device ID or PCI BDF address to query. It will show more detailed info.");
    auto pfOpt = addFlag("--pf, --physicalfunction", this->opts->showPfOnly, "Display the physical functions only.");
    auto vfOpt = addFlag("--vf, --virtualfunction", this->opts->showVfOnly, "Display the virtual functions only.");
    deviceIdOpt->excludes(pfOpt);
    deviceIdOpt->excludes(vfOpt);
    pfOpt->excludes(vfOpt);
    deviceIdOpt->check([](const std::string &str) {
        std::string errStr = "Device id should be a non-negative integer or a BDF string";
        if (isValidDeviceId(str)) {
            return std::string();
        } else if (isBDF(str)) {
            return std::string();
        }
        return errStr;
    });
    std::string dumpHelp = "Property ID to dump device properties in CSV format. Separated by the comma. \"-1\" means all properties.";
    for (auto& conf : dumpFieldConfig) {
        dumpHelp += "\n";
        dumpHelp += std::to_string(conf.dumpId);
        dumpHelp += ". ";
        dumpHelp += conf.label;
    }

    auto dumpOpt = addOption("--dump", this->opts->propIdList, dumpHelp);
    dumpOpt->delimiter(',');
    dumpOpt->check([](const std::string &str) {
        std::string errStr = "Invalid Device Propery ID";
        std::regex regex = std::regex("\\,");
        std::vector<std::string> ids(std::sregex_token_iterator(
                                     str.begin(), str.end(), regex, -1),
                                     std::sregex_token_iterator());
        for (auto &id: ids) {
            if (!isInteger(id)) {
                return errStr;
            }

            auto propId = std::stoi(id);
            if (propId == ALL_PROP_ID) {
                continue;
            }
            
            if (getDumpPropConfig(propId) == nullptr) {
                return errStr;
            }
        }
        return std::string();
    });
    dumpOpt->excludes(deviceIdOpt);
    auto listamcversionsOpt = addFlag("--listamcversions", this->opts->listamcversions, "Show all AMC firmware versions.");
    deviceIdOpt->excludes(listamcversionsOpt);

    addOption("-u,--username", this->opts->username, "Username used to authenticate for host redfish access");
    addOption("-p,--password", this->opts->password, "Password used to authenticate for host redfish access");
}

void ComletDiscovery::checkBadDevices(nlohmann::json &deviceJsonList) {
    std::vector<std::string> list;
    if (getBdfListFromLspci(list) == false) {
        return;
    }
    for (auto bdf : list) {
        bool found = false;
        for (auto device : deviceJsonList) {
            std::string val = device["pci_bdf_address"];
            if (val.find(bdf) != std::string::npos) {
                found = true;
                break;
            }
        }
        if (found == false) {
            auto deviceJson = nlohmann::json();
            if (isShortBDF(bdf) == true) {
                bdf = "0000:" + bdf;
            }
            deviceJson["pci_bdf_address"] = bdf;
            if (isPhysicalFunctionDevice(bdf) == true) {
                deviceJson["gfx_firmware_status"] = "GPU in bad state";
                FirmwareVersion fwVer;
                if (getFirmwareVersion(fwVer, bdf) == true) {
                    deviceJson["gfx_firmware_version"] = fwVer.gfx_fw_version;
                    deviceJson["gfx_data_firmware_version"] = 
                        fwVer.gfx_data_fw_version;
                }
            }
            PciDeviceData pdd;
            if (getPciDeviceData(pdd, bdf) == true) {
                deviceJson["device_name"] = pdd.name;
                deviceJson["pci_device_id"] = pdd.pciDeviceId;
                deviceJson["pci_vendor_id"] = pdd.vendorId;
            }
            std::vector<std::string> pciPath;
            if (getPciPath(pciPath, bdf) == true) {
                std::string name = this->coreStub->getPciSlotName(pciPath);
                if (name.length() > 0) {
                    deviceJson["pci_slot"] = name;
                }
            }
            deviceJsonList.push_back(deviceJson);
        }
    }
    return;
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
        } else {
            json = this->coreStub->getDeviceProperties(this->opts->deviceId.c_str(), this->opts->username, this->opts->password);
        }
        return json;
    }

    if (this->opts->propIdList.size() > 0) {
        auto json = std::make_unique<nlohmann::json>();
        auto deviceListJson = this->coreStub->getDeviceList();
        auto deviceList = (*deviceListJson)["device_list"];
        nlohmann::json deviceJsonList;
        for (auto& device : deviceList) {
            auto deviceDetailedJson = this->coreStub->getDeviceProperties(device["device_id"], this->opts->username, this->opts->password);
            auto deviceJson = nlohmann::json::parse(deviceDetailedJson->dump());
            deviceJsonList.push_back(deviceJson);
        }
        checkBadDevices(deviceJsonList);
        (*json)["device_list"] = deviceJsonList;
        return json;
    }

    auto json = this->coreStub->getDeviceList();
    auto filtered = std::make_unique<nlohmann::json>();
    std::copy_if((*json)["device_list"].begin(), (*json)["device_list"].end(), std::back_inserter((*filtered)["device_list"]), [this](const nlohmann::json& item) {
        if (this->opts->showVfOnly) {
            return item.contains("device_function_type") && item["device_function_type"] == "virtual";
        } else {
            return item.contains("device_function_type") && item["device_function_type"] == "physical";
        }
    });
    return filtered;
}

bool ComletDiscovery::showWarnMsg(std::ostream &out){
    std::string amcWarnMsg = this->coreStub->getRedfishAmcWarnMsg();
    if (amcWarnMsg.length()) {
        out << this->coreStub->getRedfishAmcWarnMsg() << std::endl;
        out << "Do you want to continue? (y/n)" << std::endl;
        std::string confirm;
        std::cin >> confirm;
        if (confirm != "Y" && confirm != "y") {
            out << "Aborted" << std::endl;
            return false;
        }
    }
    return true;
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
    // Set FW name to empty when version was empty
    // And they won't be showed in the table of output
    nlohmann::json js = *json;
    std::string ver = js["gfx_data_firmware_version"];
    if (ver.length() == 0) {
        js["gfx_data_firmware_name"] = "";
    }
    ver = js["gfx_pscbin_firmware_version"];
    if (ver.length() == 0) {
        js["gfx_pscbin_firmware_name"] = "";
    }
    ver = js["amc_firmware_version"];
    if (ver.length() == 0) {
        js["amc_firmware_name"] = "";
    }
    ver = js["pci_device_id"];
    if (ver.length() != 0 && isATSMPlatform(ver)) {
        js["serial_number"] = XPUM_TABLE_HIDE_TAG;
        js["number_of_fabric_ports"] = XPUM_TABLE_HIDE_TAG;
        js["max_fabric_port_speed"] = XPUM_TABLE_HIDE_TAG;
        js["number_of_lanes_per_fabric_port"] = XPUM_TABLE_HIDE_TAG;
        js["oam_socket_id"] = XPUM_TABLE_HIDE_TAG;
    }
    CharTable table(ComletConfigDiscoveryDetailed, js);
    table.show(out);
}

static void dumpAllDeviceInfo(std::ostream &out, std::shared_ptr<nlohmann::json> json, 
                              std::vector<int> &propIdList) {
    bool dumpAllProp = false;
    for (size_t i = 0; i < propIdList.size(); ++i) {
        if (propIdList[i] == ALL_PROP_ID) {
            dumpAllProp = true;
            break;
        }
    }

    if (dumpAllProp) {
        propIdList.clear();
        for (auto& conf : dumpFieldConfig) {
            propIdList.push_back(conf.dumpId);
        }
    }
                                
    for (size_t i = 0; i < propIdList.size(); ++i) {
        auto prop = getDumpPropConfig(propIdList[i]);
        assert(prop != nullptr);
        if (prop == nullptr) {
            return ;
        }
        if (i > 0) {
            out << ",";
        }
        out << prop->label;
    }
    out << std::endl;

    auto devices = (*json)["device_list"];
    for (auto device : devices) {
        for (size_t i = 0; i < propIdList.size(); ++i) {
            auto prop = getDumpPropConfig(propIdList[i]);
            if (prop == nullptr) {
                return ;
            }
            if (i > 0) {
                out << ",";
            }

            auto value = device[prop->value];
            if (value == nullptr) {
                // No need to fill "" for the Device ID (number) column
                if (prop->dumpId == 1) {
                    out << "";
                } else {
                    out << "\"\"";
                }
            } else {
                if (prop->scale > 0) {
                    if (prop->suffix != "") {
                       out << "\"";
                    }
                    if (value.is_number()) {
                       out << scale_double_value(
                               std::to_string(static_cast<double>(value)),
                                                 prop->scale);
                    } else if (value.is_string()) {
                        out << scale_double_value(value, prop->scale);
                    } else {
                        out << value;
                    }
                    if (prop->suffix != "") {
                       out << prop->suffix << "\"";
                    }
                    continue;
                } else if (value.is_string() && prop->suffix != "") {
                    out << "\"" << (std::string(device[prop->value]) + 
                            prop->suffix) << "\"";
                    continue;
                } else {
                    out << device[prop->value];
                }

                if (prop->suffix != "") {
                    out << prop->suffix;
                }
            }
       }
        out << std::endl;
    }
}

static void showAmcFwVersion(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    auto versions = (*json)["amc_fw_version"];
    out << versions.size() << " AMC are found" << std::endl;
    int i = 0;
    for (auto version : versions) {
        out << "AMC " << i++ << " firmware version: " << version.get<std::string>() << std::endl;
    }
}

void ComletDiscovery::getTableResult(std::ostream &out) {
    if (this->opts->listamcversions) {
        if (!showWarnMsg(out))
            return;
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
    } else if (this->opts->propIdList.size() > 0) {
        dumpAllDeviceInfo(out, json, this->opts->propIdList);
    } else if (this->opts->deviceId.compare("-1") != 0) {
        showDetailedInfo(out, json);
        if (strcasecmp(std::string((*json)["gfx_firmware_version"]).c_str(), "unknown") == 0 ||
            strcasecmp(std::string((*json)["gfx_data_firmware_version"]).c_str(), "unknown") == 0) {
            exit_code = XPUM_CLI_ERROR_FIRMWARE_VERSION_ERROR;    
        }
    } else {
        showBasicInfo(out, json);
    }
}
} // end namespace xpum::cli
