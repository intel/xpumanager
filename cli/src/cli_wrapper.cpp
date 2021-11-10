#include "cli_wrapper.h"

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <utility>

#include "CLI/App.hpp"
#include "comlet_base.h"
#include "core_stub.h"
#include "cli_help_fmter.h"

namespace xpum::cli {

CLIWrapper::CLIWrapper(CLI::App &cliApp) : cliApp(cliApp) {
    this->opts = std::unique_ptr<CLIWrapperOptions>(new CLIWrapperOptions());

    cliApp.formatter(std::make_shared<HelpFormatter>());

    cliApp.add_flag("--raw", this->opts->raw, "Enable raw printing");
    cliApp.add_flag("--table", this->opts->table, "Print in text table instead of json");
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

void CLIWrapper::printResult(std::ostream &out) {
    for (auto comlet : comlets) {
        if (comlet->parsed()) {
            if(this->opts->table){
                comlet->getTableResult(out);
                return;
            }
            comlet->getJsonResult(out, this->opts->raw);
            return;
        }
    }
}
} // end namespace xpum::cli
