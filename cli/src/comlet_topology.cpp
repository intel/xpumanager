/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_topology.cpp
 */

#include "comlet_topology.h"

#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>

#include "cli_table.h"
#include "core_stub.h"
#include "utility.h"
#include "exit_code.h"

namespace xpum::cli {

static CharTableConfig ComletConfigTopologyDevice(R"({
    "columns": [{
        "title": "Device ID"
    }, {
        "title": "Topology Information"
    }],
    "rows": [{
        "instance": "",
        "cells": [
            "device_id", [
                { "label": "Local CPU List", "value": "affinity_localcpulist" },
                { "label": "Local CPUs", "value": "affinity_localcpus" },
                { "label": "PCIe Switch Count", "value": "switch_count" },
                { "label": "PCIe Switch", "value": "switch_list[]" }
            ]
        ]
    }]
})"_json);

void ComletTopology::setupOptions() {
    this->opts = std::unique_ptr<ComletTopologyOptions>(new ComletTopologyOptions());
    auto d = addOption("-d,--device", this->opts->device, "The device ID or PCI BDF address to query");
    auto e = addOption("-f,--file", this->opts->xmlFile,
                       "Generate the system topology with the GPU info to a XML file.");
    auto m = addFlag("-m,--matrix", this->opts->xeLink,
                     "Print the CPU/GPU topology matrix.\n"
                     "  S: Self\n"
                     "  XL[laneCount]: Two tiles on the different cards are directly connected by Xe Link.  Xe Link LAN count is also provided.\n"
                     "  XL*: Two tiles on the different cards are connected by Xe Link + MDF. They are not directly connected by Xe Link.\n"
                     "  SYS: Connected with PCIe between NUMA nodes\n"
                     "  NODE: Connected with PCIe within a NUMA node\n"
                     "  MDF: Connected with Multi-Die Fabric Interface");
    d->excludes(e);
    d->excludes(m);
    e->excludes(d);
    d->excludes(m);
    m->excludes(d);
    m->excludes(e);
}

std::unique_ptr<nlohmann::json> ComletTopology::run() {
    auto result = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (isDeviceOperation()) {
        if (this->opts->device != "") {
            if (isNumber(this->opts->device)) {
                this->opts->deviceId = std::stoi(this->opts->device);
            } else {
                auto json = this->coreStub->getDeivceIdByBDF(
                    this->opts->device.c_str(), &this->opts->deviceId);
                if (json->contains("error")) {
                    return json;
                }
            }
        }
        auto json = this->coreStub->getTopology(this->opts->deviceId);
        return json;
    } else if (!this->opts->xmlFile.empty()) {
        std::ofstream xmlfile;
        xmlfile.open(this->opts->xmlFile);
        if (xmlfile.is_open()) {
            std::string xmlBuffer = this->coreStub->getTopoXMLBuffer();
            if (!xmlBuffer.empty()) {
                xmlfile << xmlBuffer;
                std::cout << "Export topology to " << opts->xmlFile << " sucessfully." << std::endl;
            } else {
                std::cout << "Fail to get topology xml buffer." << std::endl;
                exit_code = XPUM_CLI_ERROR_EMPTY_XML;
            }
            xmlfile.close();
        } else {
            std::cout << "Error opening file: " << opts->xmlFile << std::endl;
            exit_code = XPUM_CLI_ERROR_OPEN_FILE;
        }
    } else if (this->opts->xeLink) {
        auto json = this->coreStub->getXelinkTopology();
        return json;
    } else {
        (*result)["error"] = "Wrong argument or unknow operation, run with --help for more information.";
        exit_code = XPUM_CLI_ERROR_BAD_ARGUMENT;
    }
    return result;
}

static void showDeviceTopology(std::ostream &out, std::shared_ptr<nlohmann::json> json, const bool cont = false) {
    CharTable table(ComletConfigTopologyDevice, *json, cont);
    table.show(out);
}

std::string ComletTopology::getPortList(const nlohmann::json &item) {
    std::string key = "port_list";
    std::string result = "";
    auto sub = item.find(key);
    if (sub != item.end()) {
        int portCount = 0;
        std::vector<uint32_t> portList = item[key];
        for (size_t i = 0; i < portList.size(); i++) {
            if (portList[i] > 0) {
                portCount += portList[i];
            }
        }

        if (portCount > 0) {
            result = std::to_string(portCount);
        }
    }

    return result;
}

void ComletTopology::printHead(std::string head[], int count, int headsize, int rowsize) {
    std::cout << std::left << std::setw(headsize) << " ";
    for (int i = 0; i < count; i++) {
        std::cout << std::left << std::setw(rowsize) << head[i];
    }
    std::cout << std::left << std::setw(rowsize) << "CPU Affinity" << std::endl;
}

void ComletTopology::printContent(std::string head[], const nlohmann::json &table, int count, int headsize, int rowsize) {
    for (int col = 0; col < count; col++) {
        std::cout << std::left << std::setw(headsize) << head[col];
        for (int row = 0; row < count; row++) {
            std::string linkType = getKeyStringValue("link_type", table[col * count + row]);
            if (linkType.compare("XL") == 0) {
                linkType += getPortList(table[col * count + row]);
            }
            std::cout << std::left << std::setw(rowsize) << linkType;
        }
        std::cout << std::left << std::setw(rowsize) << getKeyStringValue("local_cpu_affinity", table[col * count]);
        std::cout << std::endl;
    }
}

void ComletTopology::printXelinkTable(const nlohmann::json &table) {
    int nCount = table.size();
    int instance = sqrt(nCount);
    std::string title[instance];
    int headsize = 9;
    int rowsize = 9;

    for (int i = 0; i < instance; i++) {
        title[i] = "GPU " + getKeyNumberValue("remote_device_id", table[i]) + "/" + getKeyNumberValue("remote_subdevice_id", table[i]);
    }

    printHead(title, instance, headsize, rowsize);
    printContent(title, table, instance, headsize, rowsize);
}

void ComletTopology::showXelinkTopology(std::shared_ptr<nlohmann::json> json) {
    auto result = json->find("topo_list");
    if (result != json->end()) {
        printXelinkTable(*result);
    }
}

void ComletTopology::getTableResult(std::ostream &out) {
    auto res = run();
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        setExitCodeByJson(*res);
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;

    if (isDeviceOperation()) {
        showDeviceTopology(out, json);
    } else {
        showXelinkTopology(json);
    }
}
} // end namespace xpum::cli
