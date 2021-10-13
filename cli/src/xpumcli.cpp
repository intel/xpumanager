#include "CLI/App.hpp"
#include "cli_wrapper.h"
#include "comlet_discovery.h"
#include "comlet_group.h"
#include "comlet_topology.h"
#include "comlet_version.h"
#include "comlet_diagnostic.h"
#include "comlet_health.h"

#include <iostream>
#include <memory>
#include <string>

class ComletBase;

#define MAKE_COMLET_PTR(comlet_type) (std::static_pointer_cast<ComletBase>(std::make_shared<comlet_type>()))

int main(int argc, char **argv) {

    CLI::App app{"Intel XPU Manager Command Line Interface"};

    CLIWrapper wrapper(app);

    wrapper
        .addComlet(MAKE_COMLET_PTR(ComletVersion))
        .addComlet(MAKE_COMLET_PTR(ComletDiscovery))
        .addComlet(MAKE_COMLET_PTR(ComletTopology))
        .addComlet(MAKE_COMLET_PTR(ComletGroup))
        .addComlet(MAKE_COMLET_PTR(ComletDiagnostic))
        .addComlet(MAKE_COMLET_PTR(ComletHealth));

    app.require_subcommand();

    CLI11_PARSE(app, argc, argv);

    std::cout << wrapper.getResult() << std::endl;

    return 0;
}
