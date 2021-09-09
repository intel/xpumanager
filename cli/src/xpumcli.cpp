#include "cli_wrapper.h"
#include "comlet_discovery.h"
#include "comlet_version.h"
#include <CLI/CLI.hpp>

#define MAKE_COMLET_PTR(comlet_type) (std::static_pointer_cast<ComletBase>(std::make_shared<comlet_type>()))

int main(int argc, char **argv) {

    CLI::App app{"Intel XPU Manager Command Line Interface"};

    CLIWrapper wrapper(app);

    wrapper
        .addComlet(MAKE_COMLET_PTR(ComletVersion))
        .addComlet(MAKE_COMLET_PTR(ComletDiscovery));

    app.require_subcommand();

    CLI11_PARSE(app, argc, argv);

    return 0;
}
