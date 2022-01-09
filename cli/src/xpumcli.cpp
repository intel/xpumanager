#include <iostream>
#include <memory>
#include <string>

#include "CLI/App.hpp"
#include "cli_wrapper.h"
#include "cli_resource.h"

#include "comlet_diagnostic.h"
#include "comlet_discovery.h"
#include "comlet_group.h"
#include "comlet_health.h"
#include "comlet_policy.h"
#include "comlet_statistics.h"
#include "comlet_topology.h"
#include "comlet_config.h"
#include "comlet_reset.h"
#include "comlet_version.h"
#include "comlet_firmware.h"
#include "comlet_dump.h"
#include "comlet_agentset.h"

#include "logger.h"

#define MAKE_COMLET_PTR(comlet_type) (std::static_pointer_cast<xpum::cli::ComletBase>(std::make_shared<comlet_type>()))

int main(int argc, char **argv) {

    xpum::cli::init_logger();
    // XPUM_LOG_AUDIT("XPUM CLI (ver.%s) Started", "1.0.0.0");

    CLI::App app{xpum::cli::getResourceString("CLI_APP_DESC")};

    xpum::cli::CLIWrapper wrapper(app);
    wrapper
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletDiscovery))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletTopology))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletGroup))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletDiagnostic))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletHealth))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletPolicy))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletFirmware))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletConfig))
        //.addComlet(MAKE_COMLET_PTR(xpum::cli::ComletReset))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletStatistics))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletDump))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletAgentSet));
    app.require_subcommand(0, 1);

    if (argc == 1) {
        std::cout << app.help();
    } else {
        CLI11_PARSE(app, argc, argv);
    }

    wrapper.printResult(std::cout);

    return 0;
}
