/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file xpumcli.cpp
 */

// winxpum.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <iomanip>
#include <CLI/App.hpp>
#include <nlohmann/json.hpp>
#include "cli_wrapper.h"
#include "comlet_discovery.h"
#include "cli_resource.h"
#include "comlet_config.h"
#include "comlet_statistics.h"
#include "comlet_dump.h"

#define MAKE_COMLET_PTR(comlet_type) (std::static_pointer_cast<ComletBase>(std::make_shared<comlet_type>()))

int main(int argc, char** argv)
{
    _putenv(const_cast<char*>("ZES_ENABLE_SYSMAN=1"));

    CLI::App app{ getResourceString("CLI_APP_DESC") };

    CLIWrapper wrapper(app);
    wrapper.addComlet(MAKE_COMLET_PTR(ComletDiscovery)).
        addComlet(MAKE_COMLET_PTR(ComletStatistics)).
        addComlet(MAKE_COMLET_PTR(ComletConfig)).
        addComlet(MAKE_COMLET_PTR(ComletDump));
    app.require_subcommand(0, 1);

    if (argc == 1) {
        std::cout << app.help();
    }
    else {
        CLI11_PARSE(app, argc, argv);
    }

    wrapper.printResult(std::cout);
    return 0;
}