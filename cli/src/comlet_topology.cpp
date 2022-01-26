#include "comlet_topology.h"

#include <map>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>

#include "core_stub.h"
#include "cli_table.h"

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
    auto d = addOption("-d,--device", this->opts->deviceId, "The device ID to query");
    auto e = addOption("-f,--file", this->opts->xmlFile, 
    "Generate the system topology with the GPU info to a file. The filename determines the format of the output, including \nXML and PNG.");
    d->excludes(e);
    e->excludes(d);
}

std::unique_ptr<nlohmann::json> ComletTopology::run() {
    auto result = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (isDeviceOperation()){
        auto json = this->coreStub->getTopology(this->opts->deviceId);

        return json;
    } else if(!this->opts->xmlFile.empty()) {
        std::ofstream xmlfile;
        xmlfile.open(this->opts->xmlFile);
        std::string xmlBuffer = this->coreStub->getTopoXMLBuffer();
        if(!xmlBuffer.empty()){
            xmlfile << xmlBuffer;
            std::cout << "Export topology to xml:" << opts->xmlFile << " sucessfully." << std::endl;
        } else {
            std::cout << "Fail to get topology xml buffer." << std::endl;
        }
        xmlfile.close();        
    } else {        
        (*result)["error"] = "Wrong argument or unknow operation, run with --help for more information.";
    }
    return result;
}

static void showDeviceTopology(std::ostream &out, std::shared_ptr<nlohmann::json> json, const bool cont = false) {
    CharTable table(ComletConfigTopologyDevice, *json, cont);
    table.show(out);
}

void ComletTopology::getTableResult(std::ostream &out) {
    auto res = run();
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;

    if (isDeviceOperation()) {
        showDeviceTopology(out, json);
    }
}
} // end namespace xpum::cli
