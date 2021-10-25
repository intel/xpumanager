#include "comlet_version.h"

#include "config.h"
#include "core_stub.h"

#include <map>
#include <nlohmann/json.hpp>

namespace xpum::cli {

void ComletVersion::setupOptions() {
    this->opts = std::unique_ptr<ComletVersionOptions>(new ComletVersionOptions());
}

std::unique_ptr<nlohmann::json> ComletVersion::run() {
    auto json = this->coreStub->getVersion();
    (*json)["cli_version"] = CLI_VERSION;
    (*json)["cli_version_git"] = CLI_VERSION_GIT_COMMIT;
    return json;
}
} // end namespace xpum::cli
