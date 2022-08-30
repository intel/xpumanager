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
               "  xpumcli group -l -g [groupId] \n";
    } else if (app->get_name().compare("top") == 0) {
        return "\n"
                "Usage: xpumcli top [Options] \n"
                "  xpumcli top  \n"
                "  xpumcli top -d [deviceId] \n"
                "  xpumcli top -d [deviceId] -j \n"
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
               "Usage: xpumcli topdown [Options] \n"
               "  xpumcli topdown -d [deviceId] \n"
               "  xpumcli topdown -d [deviceId] -j \n"
               "  xpumcli topdown -d [deviceId] -t [tileId] \n"
               "  xpumcli topdown -d [deviceId] -t [tileId] -j \n"
               "\nEU in Use:               Contribution to throughput (observed) when EUs are in use with EU threads placed (higher is better)\n"
               "EU Active:               Contribution to throughput (observed) when EUs are processing instructions from some EU threads (higher is better)\n"
               "ALU Active:              Contribution to throughput (estimated) with ALU instructions being processed (higher is better)\n"
               "FPU active:              Contribution to throughput (estimated) with floating-point or int64 instructions being processed (higher is better)\n"
               "Em Int Only:             Contribution to throughput (estimated) with extended math or integer instructions being processed (higher is better)\n"
               "Xmx Active:              Contribution to throughput (estimated) with Xe Matrix Extension (i.e., systolic array) instructions being processed (higher is better)\n"
               "FPU Idle:                Loss of throughput (estimated) without floating-point or int64 instructions being processed (lower is better)\n"
               "Em Int Idle:             Loss of throughput (estimated) without extended math or integer instructions being processed (lower is better)\n"
               "Xmx Idle:                Loss of throughput (estimated) without Xe Matrix Extension (systolic array) instructions being processed (lower is better)\n"
               "Other Instructions:	     Loss of throughput (estimated) without ALU instructions being processed (lower is better)\n"
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
               "  xpumcli discovery -d [deviceId] -j\n"
               "  xpumcli discovery --listamcversions\n";
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
               "  xpumcli updatefw -d [deviceId] -t GFX -f [imageFilePath]\n"
               "  xpumcli updatefw -t AMC -f [imageFilePath]\n";
    } else if (app->get_name().compare("config") == 0) {
        return "\nUsage: xpumcli config [Options]\n"
               " xpumcli config -d [deviceId]\n"
               " xpumcli config -d [deviceId] -t [tileId] --frequencyrange [minFrequency,maxFrequency]\n"
               " xpumcli config -d [deviceId] --powerlimit [powerValue]\n"
               " xpumcli config -d [deviceId] -t [tileId] --standby [standbyMode]\n"
               " xpumcli config -d [deviceId] -t [tileId] --scheduler [schedulerMode]\n"
               " xpumcli config -d [deviceId] -t [tileId] --performancefactor [engineType,factorValue]\n"
               " xpumcli config -d [deviceId] -t [tileId] --xelinkport [portId,value]\n"
               " xpumcli config -d [deviceId] -t [tileId] --xelinkportbeaconing [portId,value]\n"
               " xpumcli config -d [deviceId] --memoryecc [0|1] 0:disable; 1:enable\n";
               //" xpumcli config -d [deviceId] --reset\n";
    } else {
        return CLI::Formatter::make_usage(app, name);
    }
}

} // namespace xpum::cli
