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
#include "comlet_version.h"
#include "core_stub.h"
#include "help_formatter.h"
#include "exit_code.h"
#include "comlet_dump.h"
#include "comlet_statistics.h"
#include "comlet_diagnostic.h"
#ifndef DAEMONLESS
#include "grpc_core_stub.h"
#else
#include "lib_core_stub.h"
#endif

namespace xpum::cli {

CLIWrapper::CLIWrapper(CLI::App &cliApp, bool privilege) : cliApp(cliApp) {
    this->opts = std::unique_ptr<CLIWrapperOptions>(new CLIWrapperOptions());

    cliApp.formatter(std::make_shared<HelpFormatter>());

    cliApp.add_flag("-v, --version", this->opts->version, "Display version information and exit.");

    cliApp.fallthrough(true);

#ifndef DAEMONLESS
    this->coreStub = std::make_shared<GrpcCoreStub>(privilege);
#endif
}

CLIWrapper &CLIWrapper::addComlet(const std::shared_ptr<ComletBase> &comlet) {
    comlet->subCLIApp = this->cliApp.add_subcommand(comlet->command, comlet->description);
    comlet->subCLIApp->add_flag("-j,--json", this->opts->json, "Print result in JSON format\n");
    comlet->setupOptions();

#ifndef DAEMONLESS
    if (comlet->coreStub == nullptr) {
        comlet->coreStub = this->coreStub;
    }
#endif    

    comlets.push_back(comlet);

    return *this;
}

int CLIWrapper::printResult(std::ostream &out) {
    auto versionOpt = this->cliApp.get_option("-v");
    if (!versionOpt->empty()) {
        ComletVersion comlet;
#ifdef DAEMONLESS
        putenv(const_cast<char *>("_XPUM_INIT_SKIP=FIRMWARE"));
        this->coreStub = std::make_shared<LibCoreStub>();
#endif
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
#ifdef DAEMONLESS
            putenv(const_cast<char *>("XPUM_DISABLE_PERIODIC_METRIC_MONITOR=1"));
            if (comlet->getCommand().compare("discovery") == 0) {
                if (comlet->isEmpty()) {
                    putenv(const_cast<char *>("_XPUM_INIT_SKIP=FIRMWARE"));
                }
            } else if (comlet->getCommand().compare("updatefw") != 0 &&
                    comlet->getCommand().compare("config") != 0) {
                putenv(const_cast<char *>("_XPUM_INIT_SKIP=FIRMWARE"));
            }

            if (comlet->getCommand().compare("stats") == 0) {
                std::shared_ptr<ComletStatistics> stats_comlet = std::dynamic_pointer_cast<ComletStatistics>(comlet);
                if (stats_comlet->hasEUMetrics()){
                    if(stats_comlet->hasRASMetrics())
                        setenv("XPUM_METRICS", "0-31,36-38", 1);
                    else
                        setenv("XPUM_METRICS", "0-19,29-31,36-38", 1);
                }
                else{
                    if(stats_comlet->hasRASMetrics())
                        setenv("XPUM_METRICS", "0,4-31,36-38", 1);
                    else
                        setenv("XPUM_METRICS", "0,4-19,29-31,36-38", 1);
                }
            }
            if (comlet->getCommand().compare("dump") == 0) {
                putenv(const_cast<char *>("XPUM_DISABLE_PERIODIC_METRIC_MONITOR=0"));
                std::shared_ptr<ComletDump> dump_comlet = std::dynamic_pointer_cast<ComletDump>(comlet);
                if (dump_comlet->dumpPCIeMetrics() && dump_comlet->dumpEUMetrics())
                    setenv("XPUM_METRICS", "0-38", 1);
                else if (dump_comlet->dumpPCIeMetrics())
                    setenv("XPUM_METRICS", "0,4-38", 1);
                else if (dump_comlet->dumpEUMetrics())
                    setenv("XPUM_METRICS", "0-31,36-38", 1);
                else
                    setenv("XPUM_METRICS", "0,4-31,36-38", 1);
            }
            if (comlet->getCommand().compare("dump") == 0 && std::dynamic_pointer_cast<ComletDump>(comlet)->dumpIdlePowerOnly()) {
                this->coreStub = std::make_shared<LibCoreStub>(false);
            } else if (comlet->getCommand().compare("diag") == 0 && std::dynamic_pointer_cast<ComletDiagnostic>(comlet)->isPreCheck()) {
                this->coreStub = std::make_shared<LibCoreStub>(false);  
            } else if (comlet->getCommand().compare("log") == 0) {
                this->coreStub = std::make_shared<LibCoreStub>(false);  
            } else {
                this->coreStub = std::make_shared<LibCoreStub>();  
            }
            comlet->coreStub = this->coreStub;
#endif
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
