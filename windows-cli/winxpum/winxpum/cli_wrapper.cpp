/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file cli_wrapper.cpp
 */

#include "cli_wrapper.h"

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <utility>

#include "CLI/App.hpp"
#include "comlet_base.h"
#include "core_stub.h"
#include "help_formatter.h"
#include "comlet_version.h"

CLIWrapper::CLIWrapper(CLI::App& cliApp) : cliApp(cliApp) {
    this->opts = std::unique_ptr<CLIWrapperOptions>(new CLIWrapperOptions());

    cliApp.formatter(std::make_shared<HelpFormatter>());

    // cliApp.add_flag("--raw", this->opts->raw, "Print json output in raw format");
    cliApp.add_flag("-v, --version", this->opts->version, "Display version information and exit.");

    cliApp.fallthrough(true);

    this->coreStub = std::make_shared<CoreStub>();
}

CLIWrapper& CLIWrapper::addComlet(const std::shared_ptr<ComletBase>& comlet) {
    comlet->subCLIApp = this->cliApp.add_subcommand(comlet->command, comlet->description);
    comlet->subCLIApp->add_flag("-j,--json", this->opts->json, "Print result in JSON format\n");
    comlet->setupOptions();

    if (comlet->coreStub == nullptr) {
        comlet->coreStub = this->coreStub;
    }

    comlets.push_back(comlet);

    return *this;
}

void CLIWrapper::printResult(std::ostream& out) {
    auto versionOpt = this->cliApp.get_option("-v");
    if (!versionOpt->empty()) {
        ComletVersion comlet;
        comlet.coreStub = this->coreStub;
        comlet.getTableResult(out);
        return;
    }

    for (auto comlet : comlets) {
        if (comlet->parsed()) {
            if (comlet->printHelpWhenNoArgs && comlet->isEmpty()) {
                out << comlet->subCLIApp->help();
                return;
            }
            if (this->opts->json) {
                comlet->getJsonResult(out, this->opts->raw);
                return;
            }
            comlet->getTableResult(out);
            return;
        }
    }
}
