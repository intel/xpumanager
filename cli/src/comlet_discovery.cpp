#include "comlet_discovery.h"

#include <cassert>

void ComletDiscovery::setupOptions() {
    this->opts = std::unique_ptr<ComletDiscoveryOptions>(new ComletDiscoveryOptions());
    addOption("-d,--device", this->opts->deviceId, "device id");
    addOption("-a,--argA", this->opts->a, "test a");
}

std::unique_ptr<nlohmann::json> ComletDiscovery::run() {

    // auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    auto json = this->coreStub->getDeviceList();

    // *json = {
    //     {"device", this->opts->deviceId},
    //     {"a", this->opts->a}};

    return json;
}