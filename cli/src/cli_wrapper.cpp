#include "cli_wrapper.h"

#include <iostream>

CLIWrapper::CLIWrapper(CLI::App &cliApp) : cliApp(cliApp) {
    this->opts = std::unique_ptr<CLIWrapperOptions>(new CLIWrapperOptions());
    cliApp.add_flag("--pretty", this->opts->pretty, "Enable pretty-printing");
    cliApp.fallthrough(true);
}

CLIWrapper& CLIWrapper::addComlet(const std::shared_ptr<ComletBase> &comlet) {
    comlet->subCLIApp = this->cliApp.add_subcommand(comlet->command);
    comlet->setupOptions();

    comlet->subCLIApp->callback([comlet, this] {
        auto json = comlet->run();
        if (this->opts->pretty) {
            std::cout << json->dump(4) << std::endl;
        } else {
            std::cout << json->dump() << std::endl;
        }
    });

    return *this;
}
