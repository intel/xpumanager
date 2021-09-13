#include "comlet_version.h"

#include "config.h"

void ComletVersion::setupOptions() {
    this->opts = std::unique_ptr<ComletVersionOptions>(new ComletVersionOptions());
}

std::unique_ptr<nlohmann::json> ComletVersion::run() {
    auto json = this->coreStub->getVersion();
    (*json)["cli_version"] = XPUM_VERSION;
    return json;
}