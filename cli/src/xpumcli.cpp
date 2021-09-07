#include "cli_wrapper.h"
#include "comlet_version.h"
#include <CLI/CLI.hpp>

int main(int argc, char **argv) {

    CLI::App app{"Intel XPU Manager Command Line Interface"};

    CLIWrapper wrapper(app);

    ComletVersion comletVersion;
    wrapper.addComlet(comletVersion);

    app.require_subcommand();

    CLI11_PARSE(app, argc, argv);

    return 0;
}
