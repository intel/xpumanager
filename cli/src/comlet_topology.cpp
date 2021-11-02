#include "comlet_topology.h"

#include <map>
#include <nlohmann/json.hpp>

#include "core_stub.h"

namespace xpum::cli {

void ComletTopology::setupOptions() {
    this->opts = std::unique_ptr<ComletTopologyOptions>(new ComletTopologyOptions());
    addOption("-d,--device", this->opts->deviceId, "device id", true);
}

std::unique_ptr<nlohmann::json> ComletTopology::run() {
    auto json = this->coreStub->getTopology(this->opts->deviceId);

    return json;
}
} // end namespace xpum::cli
