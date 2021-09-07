#include "cli_wrapper.h"

CLIWrapper::CLIWrapper(CLI::App &cliApp) : cliApp(cliApp) {
}

void CLIWrapper::addComlet(ComletBase &comlet) {
    comlet.subCLIApp = this->cliApp.add_subcommand(comlet.command);
    comlet.setupOptions();
    comlet.subCLIApp->callback([&comlet] { comlet.run(); });
}
