/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file help_formatter.cpp
 */


#include "help_formatter.h"

#include "comlet_base.h"



std::string HelpFormatter::make_option_opts(const CLI::Option*) const {
    return "";
}

std::string HelpFormatter::make_usage(const CLI::App* app, std::string name) const {
    if (name.compare("") == 0) {
        return "\nUsage: xpumcli [Options]\n"
            "  xpumcli -v\n"
            "  xpumcli -h\n"
            "  xpumcli discovery\n";
    }
    else if (app->get_name().compare("dump") == 0) {
        return "\nUsage: xpumcli dump [Options]\n"
            "  xpumcli dump -d [deviceId] -t [deviceTileId] -m [metricsIds] -i [timeInterval] -n [dumpTimes]\n"
            "  xpumcli dump -d [deviceId] -t [deviceTileId] -m [metricsIds] --file [filename]\n";
    }
    else if (app->get_name().compare("stats") == 0) {
        return "\nUsage: xpumcli stats [Options]\n"
            "  xpumcli stats\n"
            "  xpumcli stats -d [deviceId]\n";
    }
    else if (app->get_name().compare("agentset") == 0) {
        return "\nUsage: xpumcli agentset [Options]\n"
            "  xpumcli agentset -l\n"
            "  xpumcli agentset -l -j\n"
            "  xpumcli agentset -t 200\n";
    }
    else if (app->get_name().compare("discovery") == 0) {
        return "\nUsage: xpumcli discovery [Options]\n"
            "  xpumcli discovery\n"
            "  xpumcli discovery -d [deviceId]\n"
            "  xpumcli discovery -d [deviceId] -j\n";
    }
    else if (app->get_name().compare("config") == 0) {
        return "\nUsage: xpumcli config [Options]\n"
            " xpumcli config -d [deviceId]\n"
            " xpumcli config -d [deviceId] -t [tileId] --frequencyrange [minFrequency,maxFrequency]\n"
            " xpumcli config -d [deviceId] --powerlimit [powerValue]\n"
            " xpumcli config -d [deviceId] --memoryecc [0|1] 0:disable; 1:enable\n"
            " xpumcli config -d [deviceId] -t [tileId] --standby [standbyMode]\n"
            " xpumcli config -d [deviceId] -t [tileId] --scheduler [schedulerMode]\n"
            " xpumcli config -d [deviceId] -t [tileId] --performancefactor [engineType,factorValue]\n"
            " xpumcli config -d [deviceId] -t [tileId] --xelinkport [portId,value]\n"
            " xpumcli config -d [deviceId] -t [tileId] --xelinkportbeaconing [portId,value]\n";
    } else if (app->get_name().compare("updatefw") == 0) {
        return "\nUsage: xpumcli updatefw [Options]\n"
               "  xpumcli updatefw -d [deviceId] -t GFX -f [imageFilePath]\n"
               "  xpumcli updatefw -d [deviceId] -t GFX_DATA -f [imageFilePath]\n";
    }
    else {
        return CLI::Formatter::make_usage(app, name);
    }
}
