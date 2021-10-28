#include "comlet_diagnostic.h"

#include <nlohmann/json.hpp>

#include "core_stub.h"

namespace xpum::cli {

void ComletDiagnostic::setupOptions() {
    this->opts = std::unique_ptr<ComletDiagnosticOptions>(new ComletDiagnosticOptions());
    addOption("-d,--device", this->opts->deviceId, "device id");
    addOption("-g,--group", this->opts->groupId, "group id");
    addOption("-l,--level", this->opts->level, "diagnostics level. It must be 1, 2 or 3, where 1 is quick test, 2 is medium test and 3 is long test");
}

std::unique_ptr<nlohmann::json> ComletDiagnostic::run() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (this->opts->level >= 1 && this->opts->level <= 3) {
        if (this->opts->deviceId >= 0)
            json = this->coreStub->runDiagnostics(this->opts->deviceId, this->opts->level);
        else if (this->opts->groupId > 0)
            json = this->coreStub->runDiagnosticsByGroup(this->opts->groupId, this->opts->level);
        else
            (*json)["error"] = "Wrong argument: device id or group id";    
        return json;
    }
    (*json)["error"] = "Unknown operation or wrong arguments";
    return json;
}
} // end namespace xpum::cli
