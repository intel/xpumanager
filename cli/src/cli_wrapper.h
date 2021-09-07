#pragma once

#include "comlet_base.h"
#include <CLI/CLI.hpp>

class ComletBase;

class CLIWrapper {

  public:
    CLIWrapper(CLI::App &cliApp);
    void addComlet(ComletBase &comlet);

  private:
    CLI::App &cliApp;
};