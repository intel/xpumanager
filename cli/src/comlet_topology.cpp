#include "comlet_topology.h"

#include <map>
#include <nlohmann/json.hpp>

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
                { "label": "PCIe Swicth", "value": "switch_list[]" }
            ]
        ]
    }]
})"_json);

void ComletTopology::setupOptions() {
    this->opts = std::unique_ptr<ComletTopologyOptions>(new ComletTopologyOptions());
    addOption("-d,--device", this->opts->deviceId, "The device ID to query", true);
}

std::unique_ptr<nlohmann::json> ComletTopology::run() {
    auto json = this->coreStub->getTopology(this->opts->deviceId);

    return json;
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
