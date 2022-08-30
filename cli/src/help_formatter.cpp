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
    if (app->get_parent() == nullptr) {
        return "\nUsage: " + name + " [Options]\n"
               "  " + name + " -v\n"
               "  " + name + " -h\n"
               "  " + name + " discovery\n";
    }
    std::string appName = app->get_parent()->get_name();
    if (app->get_name().compare("group") == 0) {
        return "\n"
               "Usage: " + appName + " group [Options] \n"
               "  " + appName + " group -c -n [groupName] \n"
               "  " + appName + " group -a -g [groupId] -d [deviceIds] \n"
               "  " + appName + " group -r -d [deviceIds] -g [groupId] \n"
               "  " + appName + " group -D -g [groupId] \n"
               "  " + appName + " group -l \n"
               "  " + appName + " -l -j \n"
               "  " + appName + " group -l -g [groupId] \n";
    } else if (app->get_name().compare("topology") == 0) {
#ifndef DAEMONLESS
        return "\n"
               "Usage: " + appName + " topology [Options] \n"
               "  " + appName + " topology -d [deviceId] \n"
               "  " + appName + " topology -d [deviceId] -j \n"
               "  " + appName + " topology -f [filename]  \n"
               "  " + appName + " topology -m  \n";
#else
        return "\n"
               "Usage: " + appName + " topology [Options] \n"
               "  " + appName + " topology -d [deviceId] \n"
               "  " + appName + " topology -d [pciBdfAddress] \n"
               "  " + appName + " topology -d [deviceId] -j \n"
               "  " + appName + " topology -f [filename]  \n"
               "  " + appName + " topology -m  \n";
#endif
    } else if (app->get_name().compare("health") == 0) {
        return "\nUsage: " + appName + " health [Options] \n"
               "  " + appName + " health -l \n"
               "  " + appName + " health -l -j \n"
               "  " + appName + " health -d [deviceId] \n"
               "  " + appName + " health -d [deviceId] -j \n"
               "  " + appName + " health -g [groupId] \n"
               "  " + appName + " health -g [groupId] -j \n"
               "  " + appName + " health -d [deviceId] -c [componentTypeId] --threshold [threshold] \n"
               "  " + appName + " health -d [deviceId] -c [componentTypeId] --threshold [threshold] -j \n"
               "  " + appName + " health -g [groupId] -c [componentTypeId] --threshold [threshold] \n"
               "  " + appName + " health -g [groupId] -c [componentTypeId] --threshold [threshold] -j \n";
    } else if (app->get_name().compare("diag") == 0) {
#ifndef DAEMONLESS
        return "\nUsage: " + appName + " diag [Options] \n"
               "  " + appName + " diag -d [deviceId] -l [level] \n"
               "  " + appName + " diag -d [deviceId] -l [level] -j \n"
               "  " + appName + " diag -g [groupId] -l [level] \n"
               "  " + appName + " diag -g [groupId] -l [level] -j \n";
#else
        return "\nUsage: " + appName + " diag [Options] \n"
               "  " + appName + " diag -d [deviceId] -l [level] \n"
               "  " + appName + " diag -d [pciBdfAddress] -l [level] \n"
               "  " + appName + " diag -d [deviceId] -l [level] -j \n"
               "  " + appName + " diag -d [pciBdfAddress] -l [level] -j \n";
#endif
    } else if (app->get_name().compare("dump") == 0) {
#ifndef DAEMONLESS
        return "\nUsage: " + appName + " dump [Options]\n"
               "  " + appName + " dump -d [deviceId] -t [deviceTileId] -m [metricsIds] -i [timeInterval] -n [dumpTimes]\n"
               "\n"
               "  " + appName + " dump --rawdata --start -d [deviceId] -t [deviceTileId] -m [metricsIds]\n"
               "  " + appName + " dump --rawdata --list\n"
               "  " + appName + " dump --rawdata --stop [taskId]\n";
#else
        return "\nUsage: " + appName + " dump [Options]\n"
               "  " + appName + " dump -d [deviceId] -t [deviceTileId] -m [metricsIds] -i [timeInterval] -n [dumpTimes]\n"
               "  " + appName + " dump -d [pciBdfAddress] -t [deviceTileId] -m [metricsIds] -i [timeInterval] -n [dumpTimes]\n";
#endif
    } else if (app->get_name().compare("stats") == 0) {
#ifndef DAEMONLESS
        return "\nUsage: " + appName + " stats [Options]\n"
               "  " + appName + " stats\n"
               "  " + appName + " stats -d [deviceId]\n"
               "  " + appName + " stats -g [groupId]\n";
#else
        return "\nUsage: " + appName + " stats [Options]\n"
               "  " + appName + " stats\n"
               "  " + appName + " stats -d [deviceId]\n"
               "  " + appName + " stats -d [pciBdfAddress]\n"
               "  " + appName + " stats -d [deviceId] -j\n"
               "  " + appName + " stats -d [pciBdfAddress] -j\n"
               "  " + appName + " stats -d [deviceId] -e\n"
               "  " + appName + " stats -d [pciBdfAddress] -e\n"
               "  " + appName + " stats -d [deviceId] -e -j\n"
               "  " + appName + " stats -d [pciBdfAddress] -e -j\n";
#endif
    } else if (app->get_name().compare("agentset") == 0) {
        return "\nUsage: " + appName + " agentset [Options]\n"
               "  " + appName + " agentset -l\n"
               "  " + appName + " agentset -l -j\n"
               "  " + appName + " agentset -t 200\n";
    } else if (app->get_name().compare("discovery") == 0) {
        return "\nUsage: " + appName + " discovery [Options]\n"
               "  " + appName + " discovery\n"
#ifndef DAEMONLESS
               "  " + appName + " discovery -d [deviceId]\n"
#else
               "  " + appName + " discovery -d [deviceId]\n"
               "  " + appName + " discovery -d [pciBdfAddress]\n"
#endif
               "  " + appName + " discovery -d [deviceId] -j\n"
#ifdef DAEMONLESS
               "  " + appName + " discovery --dump [propertyIds]\n";
#else
               "  " + appName + " discovery --listamcversions\n";
#endif
    } else if (app->get_name().compare("policy") == 0) {
        return "\nUsage: " + appName + " policy [Options]\n"
               "  " + appName + " policy -l\n"
               "  " + appName + " policy --listalltypes\n"
               "  " + appName + " policy -d [deviceId] -l\n"
               "  " + appName + " policy -d [deviceId] -l -j\n"
               "  " + appName + " policy -g [groupId] -l\n"
               "  " + appName + " policy -g [groupId] -l -j\n"
               "  " + appName + " policy -c -d [deviceId] --type 1 --condition 1 --threshold [threshold]  --action 1 --throttlefrequencymin [frequencyMinValue] --throttlefrequencymax [frequencyMaxValue]\n"
               "  " + appName + " policy -c -g [groupId] --type 1 --condition 1 --threshold [threshold]  --action 1 --throttlefrequencymin [frequencyMinValue] --throttlefrequencymax [frequencyMaxValue]\n"
               "  " + appName + " policy -r -d [deviceId] --type [policyTypeValue]\n"
               "  " + appName + " policy -r -g [groupId] --type [policyTypeValue]\n";
    } else if (app->get_name().compare("updatefw") == 0) {
#ifndef DAEMONLESS
        return "\nUsage: " + appName + " updatefw [Options]\n"
               "  " + appName + " updatefw -d [deviceId] -t GFX -f [imageFilePath]\n"
               "  " + appName + " updatefw -t AMC -f [imageFilePath]\n";
#else
        return "\nUsage: " + appName + " updatefw [Options]\n"
               "  " + appName + " updatefw -d [deviceId] -t GFX -f [imageFilePath]\n"
               "  " + appName + " updatefw -d [pciBdfAddress] -t GFX -f [imageFilePath]\n";
#endif
    } else if (app->get_name().compare("config") == 0) {
        return "\nUsage: " + appName + " config [Options]\n"
               " " + appName + " config -d [deviceId]\n"
               " " + appName + " config -d [deviceId] -t [tileId] --frequencyrange [minFrequency,maxFrequency]\n"
               " " + appName + " config -d [deviceId] --powerlimit [powerValue]\n"
               " " + appName + " config -d [deviceId] -t [tileId] --standby [standbyMode]\n"
               " " + appName + " config -d [deviceId] -t [tileId] --scheduler [schedulerMode]\n"
               " " + appName + " config -d [deviceId] -t [tileId] --performancefactor [engineType,factorValue]\n"
               " " + appName + " config -d [deviceId] -t [tileId] --xelinkport [portId,value]\n"
               " " + appName + " config -d [deviceId] -t [tileId] --xelinkportbeaconing [portId,value]\n"
               " " + appName + " config -d [deviceId] --memoryecc [0|1] 0:disable; 1:enable\n";
               //" " + appName + " config -d [deviceId] --reset\n";
    } else {
        return CLI::Formatter::make_usage(app, name);
    }
}

} // namespace xpum::cli
