#pragma once

#include "comlet_base.h"
#include <CLI/CLI.hpp>

#include <iostream>

class ComletBase;

struct CLIWrapperOptions {
    bool pretty;
};

class CLIWrapper {

  public:
    CLIWrapper(CLI::App &cliApp);
    CLIWrapper& addComlet(const std::shared_ptr<ComletBase> &comlet);

  private:
    CLI::App &cliApp;
    std::unique_ptr<CLIWrapperOptions> opts;
};