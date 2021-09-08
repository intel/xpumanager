#include "comlet_version.h"

#include "config.h"

void ComletVersion::setupOptions() {
    this->opts = std::make_shared<ComletVersionOptions>();
    addFlag("-v,--verbose", this->opts->verbose, "flag for verbose output");
    addOption("-a, --argA", this->opts->argA, "one string arg", true);
    addOption("-b, --argB", this->opts->argB, "one more string arg", false);
    addOption("-n, --times", this->opts->times, "integer arg", false);
}

std::shared_ptr<nlohmann::json> ComletVersion::run() {

    auto json = std::make_shared<nlohmann::json>();

    *json = {
        {"CLIVersion", XPUM_VERSION},
        {"Verbose", this->opts->verbose},
        {"argA", this->opts->argA},
        {"argB", this->opts->argB},
        {"times", this->opts->times}};

    return json;
}