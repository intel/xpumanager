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
#include "comlet_config.h"
#include "comlet_reset.h"
#include "comlet_version.h"
#include "comlet_firmware.h"
#include "comlet_dump.h"
#include "comlet_agentset.h"

#include "logger.h"

#define MAKE_COMLET_PTR(comlet_type) (std::static_pointer_cast<xpum::cli::ComletBase>(std::make_shared<comlet_type>()))

int main(int argc, char **argv) {

    xpum::cli::Logger::init();

    CLI::App app{R"(Intel XPU Manager Command Line Interface -- v1.0 
Intel XPU Manager Command Line Interface provides the Intel datacenter GPU model and monitoring capabilities. It can also be used to change the Intel datacenter GPU settings and update the firmware.  
Intel XPU Manager is based on Intel oneAPI Level Zero. Before using Intel XPU Manager, the GPU driver and Intel oneAPI Level Zero should be installed rightly.  
 
Supported devcies: 
  - Intel ATS-M1/ATS-M3/ATS-P)"};

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
