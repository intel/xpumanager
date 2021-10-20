#include "comlet_statistics.h"

#include "core_stub.h"

#include <map>
#include <nlohmann/json.hpp>

void ComletStatistics::setupOptions() {
    this->opts = std::unique_ptr<ComletStatisticsOptions>(new ComletStatisticsOptions());
    addOption("-d,--device", this->opts->deviceId, "The device id to query");
    addOption("-g,--group", this->opts->groupId, "The group id to query");
}

std::unique_ptr<nlohmann::json> ComletStatistics::run() {

    if (this->opts->deviceId != -1) {
        auto json = this->coreStub->getDeviceProperties(this->opts->deviceId);
        return json;
    }

    auto json = this->coreStub->getDeviceList();
    return json;
}