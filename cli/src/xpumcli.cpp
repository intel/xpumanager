/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file xpumcli.cpp
 */

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <string>

#include "CLI/App.hpp"
#include "cli_resource.h"
#include "cli_wrapper.h"
#include "comlet_agentset.h"
#include "comlet_config.h"
#include "comlet_diagnostic.h"
#include "comlet_discovery.h"
#include "comlet_dump.h"
#include "comlet_firmware.h"
#include "comlet_group.h"
#include "comlet_health.h"
#include "comlet_policy.h"
#include "comlet_reset.h"
#include "comlet_statistics.h"
#include "comlet_topology.h"
#include "comlet_version.h"
#include "core_stub.h"
#include "logger.h"

#define MAKE_COMLET_PTR(comlet_type) (std::static_pointer_cast<xpum::cli::ComletBase>(std::make_shared<comlet_type>()))

bool permissionCheck() {
    uid_t uid = getuid();
    if (uid == 0) {
        return true;
    }
    struct passwd* pw = getpwuid(uid);
    if (pw == NULL) {
        perror("getpwuid error");
    }
    int ngroups = 0;
    getgrouplist(pw->pw_name, pw->pw_gid, NULL, &ngroups);
    if (ngroups == 0) {
        return false;
    }
    gid_t groups[ngroups];
    getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups);
    std::string xpum_grp("xpum");
    bool has_permission = false;
    for (int i = 0; i < ngroups; i++) {
        struct group* gr = getgrgid(groups[i]);
        if (gr == NULL) {
            perror("getgrgid error");
        }
        std::string grp_name(gr->gr_name);
        if (grp_name == xpum_grp) {
            has_permission = true;
        }
    }
    return has_permission;
}

bool serviceStatusCheck() {
    FILE* f = popen("service xpum status", "r");
    char c_line[1024];
    while (fgets(c_line, 1024, f) != NULL) {
        std::string line(c_line);
        if (line.find("active (running)") != std::string::npos) {
            return true;
        }
    }
    pclose(f);
    return false;
}

bool levelZeroLoaderCheck() {
    FILE* f = popen("ldd /opt/xpum/bin/xpumd", "r");
    char c_line[1024];
    while (fgets(c_line, 1024, f) != NULL) {
        std::string line(c_line);
        if (line.find("libze_loader.so") != std::string::npos) {
            if (line.find("not found")) {
                return false;
            }
            return true;
        }
    }
    pclose(f);
    return true;
}

int main(int argc, char** argv) {
    xpum::cli::init_logger();
    // XPUM_LOG_AUDIT("XPUM CLI (ver.%s) Started", "1.0.0.0");

    if (!permissionCheck()) {
        std::cout << "Error: User Permission Error\r\n";
        return 0;
    }

    CLI::App app{xpum::cli::getResourceString("CLI_APP_DESC")};

    xpum::cli::CLIWrapper wrapper(app);
    if (!wrapper.getCoreStub()->isChannelReady()) {
        std::cout << "Error: XPUM Service Status Error. ";
        if (!levelZeroLoaderCheck()) {
            std::cout << "Cannot find level zero loader.";
        }
        std::cout << std::endl;
        return 0;
    }

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
