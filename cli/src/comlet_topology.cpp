#include "comlet_topology.h"

#include "core_stub.h"

#include <map>
#include <nlohmann/json.hpp>

void ComletTopology::setupOptions() {
    this->opts = std::unique_ptr<ComletTopologyOptions>(new ComletTopologyOptions());
    addOption("-d,--device", this->opts->deviceId, "device id");
}

std::unique_ptr<nlohmann::json> ComletTopology::run() {    

    auto json = this->coreStub->getTopology(this->opts->deviceId);

    return json;
}