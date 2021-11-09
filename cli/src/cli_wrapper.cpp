#include "cli_wrapper.h"

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <utility>

#include "CLI/App.hpp"
#include "comlet_base.h"
#include "core_stub.h"

namespace xpum::cli {

CLIWrapper::CLIWrapper(CLI::App &cliApp) : cliApp(cliApp) {
    this->opts = std::unique_ptr<CLIWrapperOptions>(new CLIWrapperOptions());
    cliApp.add_flag("--raw", this->opts->raw, "Enable raw printing");
    cliApp.fallthrough(true);

    this->coreStub = std::make_shared<CoreStub>();
}

CLIWrapper &CLIWrapper::addComlet(const std::shared_ptr<ComletBase> &comlet) {
    comlet->subCLIApp = this->cliApp.add_subcommand(comlet->command, comlet->description);
    comlet->setupOptions();

    if (comlet->coreStub == nullptr) {
        comlet->coreStub = this->coreStub;
    }

    comlets.push_back(comlet);

    return *this;
}

void CLIWrapper::printResult() {
    for (auto comlet : comlets) {
        if (comlet->parsed()) {
            comlet->printResult(this->opts->raw);
            return;
        }
    }
}
} // end namespace xpum::cli
