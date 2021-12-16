#include "comlet_diagnostic.h"

#include <nlohmann/json.hpp>

#include "core_stub.h"
#include "cli_table.h"

namespace xpum::cli {

void ComletDiagnostic::setupOptions() {
    this->opts = std::unique_ptr<ComletDiagnosticOptions>(new ComletDiagnosticOptions());
    addOption("-d,--device", this->opts->deviceId, "The device ID");
    addOption("-g,--group", this->opts->groupId, "The group ID");
    addOption("-l,--level", this->opts->level, "The diagnostics level to run. The valid options include\n\
      1. quick test\n\
      2. medium test\n\
      3. long test");
}

std::unique_ptr<nlohmann::json> ComletDiagnostic::run() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    if (this->opts->deviceId != INT_MIN && this->opts->deviceId < 0) {
        (*json)["error"] = "device not found";
        return json;
    }

    if (this->opts->groupId == 0) {
        (*json)["error"] = "group not found";
        return json;
    }

    if (this->opts->level != INT_MIN && (this->opts->level < 1 || this->opts->level > 3)) {
        (*json)["error"] = "invalid level";
        return json;
    }
    if (this->opts->level >= 1 && this->opts->level <= 3) {
        if (this->opts->deviceId >= 0) {
            json = this->coreStub->runDiagnostics(this->opts->deviceId, this->opts->level);
            return json;
        } else if (this->opts->groupId > 0 && this->opts->groupId != UINT_MAX) {
            json = this->coreStub->runDiagnosticsByGroup(this->opts->groupId, this->opts->level);
            return json;
        }
    }
    (*json)["error"] = "Wrong argument or unknown operation, run with --help for more information.";
    return json;
}

void ComletDiagnostic::getTableResult(std::ostream &out) {
    auto res = run();
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;
}
} // end namespace xpum::cli
