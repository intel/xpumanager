#include "cli_wrapper.h"

CLIWrapper::CLIWrapper(CLI::App &cliApp) : cliApp(cliApp) {
    this->opts = std::unique_ptr<CLIWrapperOptions>(new CLIWrapperOptions());
    cliApp.add_flag("--pretty", this->opts->pretty, "Enable pretty-printing");
    cliApp.fallthrough(true);
}

CLIWrapper &CLIWrapper::addComlet(const std::shared_ptr<ComletBase> &comlet) {
    comlet->subCLIApp = this->cliApp.add_subcommand(comlet->command, comlet->description);
    comlet->setupOptions();

    comlet->subCLIApp->callback([comlet, this] {
        auto json = comlet->run();
        this->jsonResult = std::move(json);
    });

    return *this;
}

std::string CLIWrapper::getResult() {
    if (this->opts->pretty) {
        return this->jsonResult->dump(4);
    } else {
        return this->jsonResult->dump();
    }
}