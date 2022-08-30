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
#ifndef DAEMONLESS
#include "grpc_core_stub.h"
#else
#include "lib_core_stub.h"
#endif

namespace xpum::cli {

CLIWrapper::CLIWrapper(CLI::App &cliApp, bool privilege) : cliApp(cliApp) {
    this->opts = std::unique_ptr<CLIWrapperOptions>(new CLIWrapperOptions());

    cliApp.formatter(std::make_shared<HelpFormatter>());

    // cliApp.add_flag("--raw", this->opts->raw, "Print json output in raw format");
    cliApp.add_flag("-v, --version", this->opts->version, "Display version information and exit.");

    cliApp.fallthrough(true);

}

CLIWrapper &CLIWrapper::addComlet(const std::shared_ptr<ComletBase> &comlet) {
    comlet->subCLIApp = this->cliApp.add_subcommand(comlet->command, comlet->description);
    comlet->subCLIApp->add_flag("-j,--json", this->opts->json, "Print result in JSON format");
    comlet->subCLIApp->add_flag("--debug", this->opts->debug, "Print debug info\n");
    comlet->setupOptions();

    comlets.push_back(comlet);

    return *this;
}

int CLIWrapper::printResult(std::ostream &out) {
    if (!this->opts->debug) {
        // putenv(const_cast<char *>("SPDLOG_LEVEL=TRACE"));
        putenv(const_cast<char *>("SPDLOG_LEVEL=OFF"));
    }
    auto versionOpt = this->cliApp.get_option("-v");
    if (!versionOpt->empty()) {
        ComletVersion comlet;
#ifndef DAEMONLESS
        this->coreStub = std::make_shared<GrpcCoreStub>(true);
        comlet.coreStub = this->coreStub;
#else
        putenv(const_cast<char *>("_XPUM_INIT_SKIP=FIRMWARE"));
        this->coreStub = std::make_shared<LibCoreStub>();
        comlet.coreStub = this->coreStub;
#endif
        comlet.getTableResult(out);
        return comlet.exit_code;
    }

    for (auto comlet : comlets) {
        if (comlet->parsed()) {
            if (comlet->printHelpWhenNoArgs && comlet->isEmpty()) {
                out << comlet->subCLIApp->help();
                return comlet->exit_code;
            }
#ifndef DAEMONLESS
            this->coreStub = std::make_shared<GrpcCoreStub>(true);
            if (!this->coreStub->isChannelReady()) {
                std::cout << "Error: XPUM Service Status Error. ";
                if (!levelZeroLoaderCheck()) {
                    out << "Cannot find level zero loader.";
                }
                out << std::endl;
                return XPUM_CLI_ERROR_LEVEL_ZERO_INITIALIZATION_ERROR;
            }
#else
            putenv(const_cast<char *>("XPUM_DISABLE_PERIODIC_METRIC_MONITOR=1"));
            if (comlet->getCommand().compare("discovery") == 0) {
                if (comlet->isEmpty()) {
                    putenv(const_cast<char *>("_XPUM_INIT_SKIP=FIRMWARE"));
                } else {
                    putenv(const_cast<char *>("_XPUM_INIT_SKIP=AMC"));
                }
            } else if (comlet->getCommand().compare("updatefw") != 0 &&
                    comlet->getCommand().compare("config") != 0) {
                putenv(const_cast<char *>("_XPUM_INIT_SKIP=FIRMWARE"));
            } else {
                putenv(const_cast<char *>("_XPUM_INIT_SKIP=AMC"));
            }

            if (comlet->getCommand().compare("stats") == 0) {
                std::shared_ptr<ComletStatistics> stats_comlet = dynamic_pointer_cast<ComletStatistics>(comlet);
                if (stats_comlet->hasEUMetrics())
                    setenv("XPUM_METRICS", "0-31,36-37", 1);
                else
                    setenv("XPUM_METRICS", "0,4-31,36-37", 1);
            }
            if (comlet->getCommand().compare("dump") == 0) {
                putenv(const_cast<char *>("XPUM_DISABLE_PERIODIC_METRIC_MONITOR=0"));
                std::shared_ptr<ComletDump> dump_comlet = dynamic_pointer_cast<ComletDump>(comlet);
                if (dump_comlet->dumpPCIeMetrics() && dump_comlet->dumpEUMetrics())
                    setenv("XPUM_METRICS", "0-37", 1);
                else if (dump_comlet->dumpPCIeMetrics())
                    setenv("XPUM_METRICS", "0,4-37", 1);
                else if (dump_comlet->dumpEUMetrics())
                    setenv("XPUM_METRICS", "0-31,36-37", 1);
                else
                    setenv("XPUM_METRICS", "0,4-31,36-37", 1);
            }
            this->coreStub = std::make_shared<LibCoreStub>();
#endif
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

bool levelZeroLoaderCheck() {
    FILE* f = popen("ldd /opt/xpum/bin/xpumd", "r");
    char c_line[1024];
    while (fgets(c_line, 1024, f) != NULL) {
        std::string line(c_line);
        if (line.find("libze_loader.so") != std::string::npos) {
            if (line.find("not found") != std::string::npos) {
                return false;
            }
            return true;
        }
    }
    pclose(f);
    return true;
}

} // end namespace xpum::cli
