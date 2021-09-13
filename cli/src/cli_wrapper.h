#pragma once

#include "comlet_base.h"

#include <nlohmann/json.hpp>
#include <CLI/CLI.hpp>

class ComletBase;

struct CLIWrapperOptions {
    bool pretty;
};

class CLIWrapper {

  public:
    CLIWrapper(CLI::App &cliApp);
    CLIWrapper& addComlet(const std::shared_ptr<ComletBase> &comlet);
    std::string getResult();

  private:
    CLI::App &cliApp;
    std::unique_ptr<CLIWrapperOptions> opts;
    std::unique_ptr<nlohmann::json> jsonResult;
};