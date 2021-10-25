#include "comlet_policy.h"

#include "core_stub.h"

#include <nlohmann/json.hpp>

namespace xpum::cli {

void ComletPolicy::setupOptions() {
    this->opts = std::unique_ptr<ComletPolicyOptions>(new ComletPolicyOptions());
    addFlag("-l,--list", this->opts->listAll, "list all devices");
    addOption("-d,--device", this->opts->deviceId, "device id");
    addOption("-g,--group", this->opts->groupId, "group id");
    addOption("-p,--powerThreshold", this->opts->powerThreshold, "power threshold");
    addOption("-t,--thermalThreshold", this->opts->thermalThreshold, "thermal threshold");
}

std::unique_ptr<nlohmann::json> ComletPolicy::run() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (this->opts->listAll) {
        json = this->coreStub->getAllHealth();
        return json;
    }
    if (this->opts->deviceId >= 0) {
        if (this->opts->powerThreshold >= -1) {
            json = this->coreStub->setHealthConfig(this->opts->deviceId, HEALTH_POWER_LIMIT, this->opts->powerThreshold);
            return json;
        }
        if (this->opts->thermalThreshold >= -1) {
            json = this->coreStub->setHealthConfig(this->opts->deviceId, HEALTH_THEARMAL_LIMIT, this->opts->thermalThreshold);
            return json;
        }
        json = this->coreStub->getHealth(this->opts->deviceId);
        return json;
    }

    if (this->opts->groupId >= 0) {
        if (this->opts->powerThreshold >= -1) {
            json = this->coreStub->setHealthConfigByGroup(this->opts->groupId, HEALTH_POWER_LIMIT, this->opts->powerThreshold);
            return json;
        }
        if (this->opts->thermalThreshold >= -1) {
            json = this->coreStub->setHealthConfigByGroup(this->opts->groupId, HEALTH_THEARMAL_LIMIT, this->opts->thermalThreshold);
            return json;
        }
        json = this->coreStub->getHealthByGroup(this->opts->groupId);
        return json;
    }
    return json;
}
} // end namespace xpum::cli
