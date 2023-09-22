/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file cli_wrapper.cpp
 */

#include "cli_wrapper.h"

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <utility>

#include "CLI/App.hpp"
#include "exit_code.h"
#include "comlet_base.h"
#include "comlet_version.h"
#include "comlet_statistics.h"
#include "comlet_dump.h"
#include "core_stub.h"
#include "core_stub/dll_core_stub.h"
#include "help_formatter.h"

namespace xpum::cli {

    CLIWrapper::CLIWrapper(CLI::App& cliApp, bool privilege) : cliApp(cliApp) {
        this->opts = std::unique_ptr<CLIWrapperOptions>(new CLIWrapperOptions());

        cliApp.formatter(std::make_shared<HelpFormatter>());

        cliApp.add_flag("-v, --version", this->opts->version, "Display version information and exit.");

        cliApp.fallthrough(true);
    }

    CLIWrapper& CLIWrapper::addComlet(const std::shared_ptr<ComletBase>& comlet) {
        comlet->subCLIApp = this->cliApp.add_subcommand(comlet->command, comlet->description);
        comlet->subCLIApp->add_flag("-j,--json", this->opts->json, "Print result in JSON format\n");
        comlet->setupOptions();

        comlets.push_back(comlet);

        return *this;
    }

    int CLIWrapper::printResult(std::ostream& out) {
        CLI::Option *opt = nullptr;
        try {
            opt = cliApp.get_option("-v");
        } catch (...) {
            return XPUM_CLI_ERROR_GENERIC_ERROR;
        }
        if (!opt->empty()) {
            ComletVersion comlet;
            this->coreStub = std::make_shared<DllCoreStub>();
            comlet.coreStub = this->coreStub;
            comlet.getTableResult(out);
            return comlet.exit_code;
        }

        for (auto comlet : comlets) {
            if (comlet->parsed()) {
                if (comlet->printHelpWhenNoArgs && comlet->isEmpty()) {
                    out << comlet->subCLIApp->help();
                    return comlet->exit_code;
                }

                if (comlet->getCommand().compare("stats") == 0) {
                    std::shared_ptr<ComletStatistics> stats_comlet = std::dynamic_pointer_cast<ComletStatistics>(comlet);
                    if (stats_comlet->hasEUMetrics()) {
                        if (stats_comlet->hasRASMetrics())
                            _putenv(const_cast<char*>("XPUM_METRICS=0-31,36-39"));
                        else
                            _putenv(const_cast<char*>("XPUM_METRICS=0-19,29-31,36-39"));
                    } else {
                        if (stats_comlet->hasRASMetrics())
                            _putenv(const_cast<char*>("XPUM_METRICS=0,4-31,36-39"));
                        else
                            _putenv(const_cast<char*>("XPUM_METRICS=0,4-19,29-31,36-39"));
                    }
                }
                if (comlet->getCommand().compare("dump") == 0) {
                    std::shared_ptr<ComletDump> dump_comlet = std::dynamic_pointer_cast<ComletDump>(comlet);
                    if (dump_comlet->dumpEUMetrics() && dump_comlet->dumpRASMetrics())
                        _putenv(const_cast<char*>("XPUM_METRICS=0-39"));
                    else if (dump_comlet->dumpEUMetrics())
                        _putenv(const_cast<char*>("XPUM_METRICS=0-19,29-39"));
                    else if (dump_comlet->dumpRASMetrics())
                        _putenv(const_cast<char*>("XPUM_METRICS=0,4-39"));
                    else
                        _putenv(const_cast<char*>("XPUM_METRICS=0,4-19,29-39"));
                }
                this->coreStub = std::make_shared<DllCoreStub>();
                comlet->coreStub = this->coreStub;

                if (this->opts->json) {
                    comlet->getJsonResult(out, this->opts->raw);
                    return comlet->exit_code;
                }
                comlet->getTableResult(out);
                return comlet->exit_code;
            }
        }
        return XPUM_CLI_SUCCESS;
    }

    std::shared_ptr<CoreStub> CLIWrapper::getCoreStub() {
        return coreStub;
    }

} // end namespace xpum::cli
