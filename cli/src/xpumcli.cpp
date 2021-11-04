#include <iostream>
#include <memory>
#include <string>

#include "CLI/App.hpp"
#include "cli_wrapper.h"
#include "comlet_diagnostic.h"
#include "comlet_discovery.h"
#include "comlet_group.h"
#include "comlet_health.h"
#include "comlet_policy.h"
#include "comlet_statistics.h"
#include "comlet_topology.h"
#include "comlet_version.h"

#define MAKE_COMLET_PTR(comlet_type) (std::static_pointer_cast<xpum::cli::ComletBase>(std::make_shared<comlet_type>()))

int main(int argc, char **argv) {
    CLI::App app{"Intel XPU Manager Command Line Interface"};

    xpum::cli::CLIWrapper wrapper(app);
    wrapper
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletVersion))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletDiscovery))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletTopology))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletGroup))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletDiagnostic))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletHealth))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletPolicy))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletStatistics));
    app.require_subcommand();

    CLI11_PARSE(app, argc, argv);

    std::cout << wrapper.getResult() << std::endl;

    return 0;
}
