#include "comlet_statistics.h"

#include <map>
#include <nlohmann/json.hpp>

#include "core_stub.h"

namespace xpum::cli {

void ComletStatistics::setupOptions() {
    this->opts = std::unique_ptr<ComletStatisticsOptions>(new ComletStatisticsOptions());
    addOption("-d,--device", this->opts->deviceId, "The device id to query");
    addOption("-g,--group", this->opts->groupId, "The group id to query");
}

std::unique_ptr<nlohmann::json> ComletStatistics::run() {
    if (this->opts->deviceId != -1) {
        auto json = this->coreStub->getStatistics(this->opts->deviceId);
        return json;
    } else if (this->opts->groupId != -1) {
        auto json = this->coreStub->getStatisticsByGroup(this->opts->groupId);
        return json;
    }
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    (*json)["error"] = "Unknow operation";
    return json;
}
} // end namespace xpum::cli
