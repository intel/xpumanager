/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file help_formatter.cpp
 */

#include "help_formatter.h"

#include "comlet_base.h"

namespace xpum::cli {

    std::string HelpFormatter::make_option_opts(const CLI::Option*) const {
        return "";
    }

    std::string HelpFormatter::make_usage(const CLI::App* app, std::string name) const {
        std::string appName = "xpu-smi";
        if (app->get_parent() == nullptr) {
            return "\nUsage: " + appName + " [Options]\n"
                "  " + appName + " -v\n"
                "  " + appName + " -h\n"
                "  " + appName + " discovery\n";
        }
        if (app->get_name().compare("dump") == 0) {
            return "\nUsage: " + appName + " dump [Options]\n"
                "  " + appName + " dump -d [deviceIds] -t [deviceTileIds] -m [metricsIds] -i [timeInterval] -n [dumpTimes]\n"
                "  " + appName + " dump -d [pciBdfAddress] -t [deviceTileIds] -m [metricsIds] -i [timeInterval] -n [dumpTimes]\n"
                "  " + appName + " dump -d [deviceIds] -t [deviceTileIds] -m [metricsIds] --file [filename]\n"
                "  " + appName + " dump -d [pciBdfAddress] -t [deviceTileIds] -m [metricsIds] --file [filename]\n";
        }
        else if (app->get_name().compare("stats") == 0) {
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
        }
        else if (app->get_name().compare("discovery") == 0) {
            return "\nUsage: " + appName + " discovery [Options]\n"
                "  " + appName + " discovery\n"
                "  " + appName + " discovery -d [deviceId]\n"
                "  " + appName + " discovery -d [deviceId] -j\n"
                "  " + appName + " discovery --listamcversions\n"
                ;
        }
        else if (app->get_name().compare("updatefw") == 0) {
            return "\nUsage: " + appName + " updatefw [Options]\n"
                "  " + appName + " updatefw -d [deviceId] -t GFX -f [imageFilePath]\n"
                "  " + appName + " updatefw -d [pciBdfAddress] -t GFX -f [imageFilePath]\n" 
                "  " + appName + " updatefw -t AMC -f [imageFilePath]\n ";
        }
        else if (app->get_name().compare("config") == 0) {
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
        }
        else {
            return CLI::Formatter::make_usage(app, name);
        }
    }

} // namespace xpum::cli
