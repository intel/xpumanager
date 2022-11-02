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
#ifndef DAEMONLESS
    std::string appName = "xpumcli";
#else
    std::string appName = "xpu-smi";
#endif
    if (app->get_parent() == nullptr) {
        return "\nUsage: " + appName + " [Options]\n"
               "  " + appName + " -v\n"
               "  " + appName + " -h\n"
               "  " + appName + " discovery\n";
    }
    if (app->get_name().compare("group") == 0) {
        return "\n"
               "Usage: " + appName + " group [Options] \n"
               "  " + appName + " group -c -n [groupName] \n"
               "  " + appName + " group -a -g [groupId] -d [deviceIds] \n"
               "  " + appName + " group -r -d [deviceIds] -g [groupId] \n"
               "  " + appName + " group -D -g [groupId] \n"
               "  " + appName + " group -l \n"
               "  " + appName + " group -l -g [groupId] \n";
    } else if (app->get_name().compare("top") == 0) {
        return "\n"
                "Usage: " + appName + " top [Options] \n"
                "  " + appName + " top  \n"
                "  " + appName + " top -d [deviceId] \n"
                "  " + appName + " top -d [deviceId] -j \n"
                "\nPID:      Process ID\n"
                "Command:  Process command name\n"
                "DeviceID: Device ID\n"
                "%REN:     Render engine utilization\n"
                "%COM:     Compute engine utilization\n"
                "%CPY:     Copy engine utilization\n"
                "%MED:     Media engine utilization\n"
                "%MEE:     Media enhancement engine utilization\n"
                "SHR:      The size of shared device memory mapped "
                "into this process (may not necessarily be resident "
                "on the device at the time of reading) (kB)\n"
                "MEM:      Device memory size in bytes allocated by "
                "this process (may not necessarily be resident on "
                "the device at the time of reading) (kB)\n";
    } else if (app->get_name().compare("topdown") == 0) {
        return "\n"
               "Usage: " + appName + " topdown [Options] \n"
               "  " + appName + " topdown -d [deviceId] \n"
               "  " + appName + " topdown -d [deviceId] -j \n"
               "  " + appName + " topdown -d [deviceId] -t [tileId] \n"
               "  " + appName + " topdown -d [deviceId] -t [tileId] -j \n"
               "\nEU in Use:               Contribution to throughput (observed) when EUs are in use with EU threads placed (higher is better)\n"
               "EU Active:               Contribution to throughput (observed) when EUs are processing instructions from some EU threads (higher is better)\n"
               "ALU Active:              Contribution to throughput (estimated) with ALU instructions being processed (higher is better)\n"
               "FPU active:              Contribution to throughput (estimated) with floating-point or int64 instructions being processed (higher is better)\n"
               "Em Int Only:             Contribution to throughput (estimated) with extended math or integer instructions being processed (higher is better)\n"
               "Xmx Active:              Contribution to throughput (estimated) with Xe Matrix Extension (i.e., systolic array) instructions being processed (higher is better)\n"
               "FPU Idle:                Loss of throughput (estimated) without floating-point or int64 instructions being processed (lower is better)\n"
               "Em Int Idle:             Loss of throughput (estimated) without extended math or integer instructions being processed (lower is better)\n"
               "Xmx Idle:                Loss of throughput (estimated) without Xe Matrix Extension (systolic array) instructions being processed (lower is better)\n"
               "Other Instructions:      Loss of throughput (estimated) without ALU instructions being processed (lower is better)\n"
               "EU Stall:                Loss of throughput (observed) when EUs are not actively processing instructions from any EU threads (lower is better)\n"
               "Low Occupancy:           Loss of throughput (estimated) when there are not enough EU threads on EUs to hide stalls from long-latency instructions, degrading EU active percentage (lower is better)\n"
               "ALU Dep.:                Loss of throughput (estimated) when some EU threads stall due to the dependency from ALU operations, degrading EU active percentage (lower is better)\n"
               "Barrier:                 Loss of throughput (estimated) when some EU threads stall due to synchronization barriers, degrading EU active percentage (lower is better)\n"
               "Dependency/SFU/SBID:     Loss of throughput (estimated) when some EU threads stall due to different internal dependencies (e.g., memory, shared function unit, sampler, etc.), degrading EU active percentage (lower is better)\n"
               "Other(EoT):              Loss of throughput (estimated) when some EU threads stall due to other reasons such as conditional flags or End-of-Thread signals degrading EU percentage (lower is better)\n"
               "Instruction Fetch:       Loss of throughput (estimated) when some EU threads stall due to the fetch of instructions, degrading EU percentage (lower is better)\n"
               "EU Not In Use:           Loss of throughput (observed) due to the case that EUs are not used at all without EU threads placed (lower is better)\n"
               "Workload Parallelism:    Loss of throughput (estimated) due to the lack of concurrency of a workload at a time, degrading the EU usage (lower is better)\n"
               "Engine Inefficiency:     Loss of throughput (estimated) due to the inefficiency use of computer/render engines, degrading the EU usage (lower is better)\n";
    } else if (app->get_name().compare("topology") == 0) {
        return "\n"
               "Usage: " + appName + " topology [Options] \n"
               "  " + appName + " topology -d [deviceId] \n"
               "  " + appName + " topology -d [pciBdfAddress] \n"
               "  " + appName + " topology -d [deviceId] -j \n"
               "  " + appName + " topology -f [filename]  \n"
               "  " + appName + " topology -m  \n";
    } else if (app->get_name().compare("health") == 0) {
        return "\nUsage: " + appName + " health [Options] \n"
               "  " + appName + " health -l \n"
               "  " + appName + " health -l -j \n"
               "  " + appName + " health -d [deviceId] \n"
               "  " + appName + " health -d [pciBdfAddress] \n"
               "  " + appName + " health -d [deviceId] -j \n"
               "  " + appName + " health -d [pciBdfAddress] -j \n"
               "  " + appName + " health -g [groupId] \n"
               "  " + appName + " health -g [groupId] -j \n"
               "  " + appName + " health -d [deviceId] -c [componentTypeId] --threshold [threshold] \n"
               "  " + appName + " health -d [pciBdfAddress] -c [componentTypeId] --threshold [threshold] \n"
               "  " + appName + " health -d [deviceId] -c [componentTypeId] --threshold [threshold] -j \n"
               "  " + appName + " health -d [pciBdfAddress] -c [componentTypeId] --threshold [threshold] -j \n"
               "  " + appName + " health -g [groupId] -c [componentTypeId] --threshold [threshold] \n"
               "  " + appName + " health -g [groupId] -c [componentTypeId] --threshold [threshold] -j \n";
    } else if (app->get_name().compare("diag") == 0) {
#ifndef DAEMONLESS
        return "\nUsage: " + appName + " diag [Options] \n"
               "  " + appName + " diag -d [deviceId] -l [level] \n"
               "  " + appName + " diag -d [pciBdfAddress] -l [level] \n"
               "  " + appName + " diag -d [deviceId] -l [level] -j \n"
               "  " + appName + " diag -d [pciBdfAddress] -l [level] -j \n"
               "  " + appName + " diag -d [deviceIds] --stress --stresstime [time]\n"
               "  " + appName + " diag -g [groupId] -l [level] \n"
               "  " + appName + " diag -g [groupId] -l [level] -j \n"
               "  " + appName + " diag --precheck\n"
               "  " + appName + " diag --precheck -j\n"
               "  " + appName + " diag --stress --stresstime [time]\n";
#else
        return "\nUsage: " + appName + " diag [Options] \n"
               "  " + appName + " diag -d [deviceId] -l [level] \n"
               "  " + appName + " diag -d [pciBdfAddress] -l [level] \n"
               "  " + appName + " diag -d [deviceId] -l [level] -j \n"
               "  " + appName + " diag -d [pciBdfAddress] -l [level] -j \n"
               "  " + appName + " diag -d [deviceIds] --stress \n"
               "  " + appName + " diag -d [deviceIds] --stress --stresstime [time] \n"
               "  " + appName + " diag --precheck\n"
               "  " + appName + " diag --precheck -j\n"
               "  " + appName + " diag --stress\n"
               "  " + appName + " diag --stress --stresstime [time]\n";
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
               "  " + appName + " discovery -d [deviceId]\n"
               "  " + appName + " discovery -d [pciBdfAddress]\n"
               "  " + appName + " discovery -d [deviceId] -j\n"
               "  " + appName + " discovery --dump [propertyIds]\n"
#ifndef DAEMONLESS
               "  " + appName + " discovery --listamcversions\n"
#endif
               ;
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
               "  " + appName + " updatefw -d [pciBdfAddress] -t GFX -f [imageFilePath]\n"
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
    } else if (app->get_name().compare("amcsensor") == 0) {
        return "\nUsage: " + appName + " amcsensor [Options]\n"
               " " + appName + " amcsensor\n"
               " " + appName + " amcsensor -j\n";
    } else {
        return CLI::Formatter::make_usage(app, name);
    }
}

} // namespace xpum::cli
