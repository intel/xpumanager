/* 
 *  Copyright (C) 2021-2023 Intel Corporation
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
#include "comlet_ps.h"
#include "comlet_vgpu.h"
#include "comlet_log.h"
#include "comlet_topdown.h"
#include "comlet_sensor.h"
#include "core_stub.h"
#include "logger.h"
#include "exit_code.h"

#define MAKE_COMLET_PTR(comlet_type) (std::static_pointer_cast<xpum::cli::ComletBase>(std::make_shared<comlet_type>()))

bool privilegeCheck() {
    uid_t uid = getuid();
    if (uid == 0) {
        return true;
    }
    struct passwd* pw = getpwuid(uid);
    if (pw == NULL) {
        perror("getpwuid error");
        return false;
    }
    int ngroups = 0;
    getgrouplist(pw->pw_name, pw->pw_gid, NULL, &ngroups);
    if (ngroups == 0) {
        return false;
    }
    gid_t groups[ngroups];
    getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups);
    std::string xpum_grp("xpum");
    bool has_privilege = false;
    for (int i = 0; i < ngroups; i++) {
        struct group* gr = getgrgid(groups[i]);
        if (gr == NULL) {
            perror("getgrgid error");
            return false;
        }
        std::string grp_name(gr->gr_name);
        if (grp_name == xpum_grp) {
            has_privilege = true;
        }
    }

    return has_privilege;
}

bool levelZeroLoaderCheck() {
    char exe_path[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", exe_path, sizeof(exe_path));
    exe_path[len] = '\0';
    std::string current_file = exe_path;
    std::string xpumd_path = current_file.substr(0, current_file.find_last_of('/')) + "/xpumd";
    std::string command = "ldd " + xpumd_path;
    FILE* f = popen(command.c_str(), "r");
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

int main(int argc, char** argv) {
    xpum::cli::init_logger();
    // XPUM_LOG_AUDIT("XPUM CLI (ver.%s) Started", "1.0.0.0");

    bool priv = privilegeCheck();

    CLI::App app{xpum::cli::getResourceString("CLI_APP_DESC")};

#ifdef DAEMONLESS
    app.name("xpu-smi");
#endif

    xpum::cli::CLIWrapper wrapper(app, priv);
#ifndef DAEMONLESS
    bool help = false;
    for (int i = 1; !help && i < argc; i++)
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
            help = true;
    bool precheck = argc >= 3 && strcmp(argv[1], "diag") == 0 && strcmp(argv[2], "--precheck") == 0;
    if (!help && !precheck) {
        if (!wrapper.getCoreStub()->isChannelReady()) {
            std::cout << "Error: XPUM Service Status Error. ";
            if (!levelZeroLoaderCheck()) {
                std::cout << "Cannot find level zero loader.";
            }
            std::cout << std::endl;
            return 0;
        }
    }
#endif

    wrapper
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletDiscovery))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletTopology))
#ifndef DAEMONLESS
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletGroup))
#endif
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletDiagnostic))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletHealth))
#ifndef DAEMONLESS
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletPolicy))
#endif
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletFirmware))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletConfig))
#ifndef DAEMONLESS
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletTopdown))
#endif
        //.addComlet(MAKE_COMLET_PTR(xpum::cli::ComletReset))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletPs))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletVgpu))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletStatistics))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletDump))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletLog))
#ifndef DAEMONLESS
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletAgentSet))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletSensor))
#endif
        ;
    app.require_subcommand(0, 1);

    if (argc == 1) {
        std::cout << app.help();
        return XPUM_CLI_SUCCESS;
    } else {
        // CLI11_PARSE(app, argc, argv);
        try {
            app.parse(argc, argv);
        } catch (const CLI::ParseError& e) {
            auto err = app.exit(e);
            return err != 0 ? XPUM_CLI_ERROR_BAD_ARGUMENT : 0;
        }
    }

    return wrapper.printResult(std::cout);
}
