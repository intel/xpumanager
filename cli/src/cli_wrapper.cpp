#include "cli_wrapper.h"

#include <iostream>

CLIWrapper::CLIWrapper(CLI::App &cliApp) : cliApp(cliApp) {
}

void CLIWrapper::addComlet(ComletBase &comlet) {
    comlet.subCLIApp = this->cliApp.add_subcommand(comlet.command);
    comlet.setupOptions();
    comlet.subCLIApp->callback([&comlet] {
        auto json = comlet.run();
        std::cout << json->dump(4) << std::endl;
    });
}
