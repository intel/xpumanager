#include "cli_wrapper.h"

#include "CLI/App.hpp"
#include "comlet_base.h"
#include "core_stub.h"

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <utility>

CLIWrapper::CLIWrapper(CLI::App &cliApp) : cliApp(cliApp) {
    this->opts = std::unique_ptr<CLIWrapperOptions>(new CLIWrapperOptions());
    cliApp.add_flag("--raw", this->opts->raw, "Enable raw printing");
    cliApp.fallthrough(true);

    this->coreStub = std::make_shared<CoreStub>();
}

CLIWrapper &CLIWrapper::addComlet(const std::shared_ptr<ComletBase> &comlet) {
    comlet->subCLIApp = this->cliApp.add_subcommand(comlet->command, comlet->description);
    comlet->setupOptions();

    comlet->subCLIApp->callback([comlet, this] {
        auto json = comlet->run();
        this->jsonResult = std::move(json);
    });

    if (comlet->coreStub == nullptr) {
        comlet->coreStub = this->coreStub;
    }

    return *this;
}

std::string CLIWrapper::getResult() {
    if (this->opts->raw) {
        return this->jsonResult->dump();
    } else {
        return this->jsonResult->dump(4);
    }
}