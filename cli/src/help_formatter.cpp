/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file help_formatter.cpp
 */

#include "help_formatter.h"

#include "comlet_base.h"

namespace xpum::cli {

std::string HelpFormatter::make_option_opts(const CLI::Option *) const {
    return "";
}

std::string HelpFormatter::make_usage(const CLI::App *app, std::string name) const {
    if (name.compare("") == 0) {
        return "\nUsage: xpumcli [Options]\n"
               "  xpumcli -v\n"
               "  xpumcli -h\n"
               "  xpumcli discovery\n";
    } else if (app->get_name().compare("group") == 0) {
        return "\n"
               "Usage: xpumcli group [Options] \n"
               "  xpumcli group -c -n [groupName] \n"
               "  xpumcli group -a -g [groupId] -d [deviceIds] \n"
               "  xpumcli group -r -d [deviceIds] -g [groupId] \n"
               "  xpumcli group -D -g [groupId] \n"
               "  xpumcli group -l \n"
               "  xpumcli -l -j \n"
               "  xpumcli group -l -g [groupId] \n";
    } else if (app->get_name().compare("topology") == 0) {
        return "\n"
               "Usage: xpumcli topology [Options] \n"
               "  xpumcli topology -d [deviceId] \n"
               "  xpumcli topology -d [deviceId] -j \n"
               "  xpumcli topology -f [filename]  \n"
               "  xpumcli topology -m  \n";
        ;
    } else if (app->get_name().compare("health") == 0) {
        return "\nUsage: xpumcli health [Options] \n"
               "  xpumcli health -l \n"
               "  xpumcli health -l -j \n"
               "  xpumcli health -d [deviceId] \n"
               "  xpumcli health -d [deviceId] -j \n"
               "  xpumcli health -g [groupId] \n"
               "  xpumcli health -g [groupId] -j \n"
               "  xpumcli health -d [deviceId] -c [componentTypeId] --threshold [threshold] \n"
               "  xpumcli health -d [deviceId] -c [componentTypeId] --threshold [threshold] -j \n"
               "  xpumcli health -g [groupId] -c [componentTypeId] --threshold [threshold] \n"
               "  xpumcli health -g [groupId] -c [componentTypeId] --threshold [threshold] -j \n";
    } else if (app->get_name().compare("diag") == 0) {
        return "\nUsage: xpumcli diag [Options] \n"
               "  xpumcli diag -d [deviceId] -l [level] \n"
               "  xpumcli diag -d [deviceId] -l [level] -j \n"
               "  xpumcli diag -g [groupId] -l [level] \n"
               "  xpumcli diag -g [groupId] -l [level] -j \n";
    } else if (app->get_name().compare("dump") == 0) {
        return "\nUsage: xpumcli dump [Options]\n"
               "  xpumcli dump -d [deviceId] -t [deviceTileId] -m [metricsIds] -i [timeInterval] -n [dumpTimes]\n"
               "\n"
               "  xpumcli dump --rawdata --start -d [deviceId] -t [deviceTileId] -m [metricsIds]\n"
               "  xpumcli dump --rawdata --list\n"
               "  xpumcli dump --rawdata --stop [taskId]\n";
    } else if (app->get_name().compare("stats") == 0) {
        return "\nUsage: xpumcli stats [Options]\n"
               "  xpumcli stats\n"
               "  xpumcli stats -d [deviceId]\n"
               "  xpumcli stats -g [groupId]\n";
    } else if (app->get_name().compare("agentset") == 0) {
        return "\nUsage: xpumcli agentset [Options]\n"
               "  xpumcli agentset -l\n"
               "  xpumcli agentset -l -j\n"
               "  xpumcli agentset -t 200\n";
    } else if (app->get_name().compare("discovery") == 0) {
        return "\nUsage: xpumcli discovery [Options]\n"
               "  xpumcli discovery\n"
               "  xpumcli discovery -d [deviceId]\n"
               "  xpumcli discovery -d [deviceId] -j\n";
    } else if (app->get_name().compare("policy") == 0) {
        return "\nUsage: xpumcli policy [Options]\n"
               "  xpumcli policy -l\n"
               "  xpumcli policy --listalltypes\n"
               "  xpumcli policy -d [deviceId] -l\n"
               "  xpumcli policy -d [deviceId] -l -j\n"
               "  xpumcli policy -g [groupId] -l\n"
               "  xpumcli policy -g [groupId] -l -j\n"
               "  xpumcli policy -c -d [deviceId] --type 1 --condition 1 --threshold [threshold]  --action 1 --throttlefrequencymin [frequencyMinValue] --throttlefrequencymax [frequencyMaxValue]\n"
               "  xpumcli policy -c -g [groupId] --type 1 --condition 1 --threshold [threshold]  --action 1 --throttlefrequencymin [frequencyMinValue] --throttlefrequencymax [frequencyMaxValue]\n"
               "  xpumcli policy -r -d [deviceId] --type [policyTypeValue]\n"
               "  xpumcli policy -r -g [groupId] --type [policyTypeValue]\n";
    } else if (app->get_name().compare("updatefw") == 0) {
        return "\nUsage: xpumcli updatefw [Options]\n"
               "  xpumcli updatefw -d [deviceId] -t [firmwareName] -f [imageFilePath]\n"
               "  xpumcli updatefw -d [deviceId] -t [firmwareName] -f [imageFilePath] -j\n";
    } else if (app->get_name().compare("config") == 0) {
        return "\nUsage: xpumcli config [Options]\n"
               " xpumcli config -d [deviceId]\n"
               " xpumcli config -d [deviceId] -t [tileId] --frequencyrange [minFrequency,maxFrequency]\n"
               " xpumcli config -d [deviceId] --powerlimit [powerValue,averageWindow]\n"
               " xpumcli config -d [deviceId] -t [tileId] --standby [standbyMode]\n"
               " xpumcli config -d [deviceId] -t [tileId] --scheduler [schedulerMode]\n"
               " xpumcli config -d [deviceId] -t [tileId] --performancefactor [engineType,factorValue]\n"
               " xpumcli config -d [deviceId] -t [tileId] --xelinkport [portId,value]\n"
               " xpumcli config -d [deviceId] -t [tileId] --xelinkportbeaconing [portId,value]\n";
        //" xpumcli config -d [deviceId] --reset\n";
    } else {
        return CLI::Formatter::make_usage(app, name);
    }
}

} // namespace xpum::cli