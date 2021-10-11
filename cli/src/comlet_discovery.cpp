#include "comlet_discovery.h"

#include <cassert>

void ComletDiscovery::setupOptions() {
    this->opts = std::unique_ptr<ComletDiscoveryOptions>(new ComletDiscoveryOptions());
    addOption("-d,--device", this->opts->deviceId, "device id");
}

std::unique_ptr<nlohmann::json> ComletDiscovery::run() {

    if (this->opts->deviceId != -1) {
        auto json = this->coreStub->getDeviceProperties(this->opts->deviceId);
        return json;
    }

    auto json = this->coreStub->getDeviceList();
    return json;
}