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
    void addComlet(ComletBase &comlet);

  private:
    CLI::App &cliApp;
    std::unique_ptr<CLIWrapperOptions> opts;
};