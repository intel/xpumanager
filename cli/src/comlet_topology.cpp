#include "comlet_topology.h"

void ComletTopology::setupOptions() {
    this->opts = std::unique_ptr<ComletTopologyOptions>(new ComletTopologyOptions());
    addOption("-d,--device", this->opts->deviceId, "device id");
}

std::unique_ptr<nlohmann::json> ComletTopology::run() {
    //auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    DeviceId device_id;
    device_id.set_id(this->opts->deviceId);

    auto json = this->coreStub->getTopology(device_id);

    return json;
}