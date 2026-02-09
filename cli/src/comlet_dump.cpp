/* 
 *  Copyright (C) 2021-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_dump.cpp
 */

#include "comlet_dump.h"

#include <map>
#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include "core_stub.h"
#include "pretty_table.h"
#include "xpum_structs.h"
#include "utility.h"
#include "exit_code.h"
#include "local_functions.h"

using xpum::dump::engineNameMap;

namespace xpum::cli {

// Check if a metric requires MEI device access for telemetry
static bool requiresMeiAccess(int metricId) {
    // Metrics requiring MEI device access: 0, 3, 4, 6, 7, 9, 10, 11, 17, 22-27, 31-34
    switch (metricId) {
        case XPUM_DUMP_GPU_UTILIZATION:                    // 0
        case XPUM_DUMP_GPU_CORE_TEMPERATURE:               // 3
        case XPUM_DUMP_MEMORY_TEMPERATURE:                 // 4
        case XPUM_DUMP_MEMORY_READ_THROUGHPUT:             // 6
        case XPUM_DUMP_MEMORY_WRITE_THROUGHPUT:            // 7
        case XPUM_DUMP_EU_ACTIVE:                          // 9
        case XPUM_DUMP_EU_STALL:                           // 10
        case XPUM_DUMP_EU_IDLE:                            // 11
        case XPUM_DUMP_MEMORY_BANDWIDTH:                   // 17
        case XPUM_DUMP_COMPUTE_ENGINE_UTILIZATION:         // 22
        case XPUM_DUMP_RENDER_ENGINE_UTILIZATION:          // 23
        case XPUM_DUMP_DECODE_ENGINE_UTILIZATION:          // 24
        case XPUM_DUMP_ENCODE_ENGINE_UTILIZATION:          // 25
        case XPUM_DUMP_COPY_ENGINE_UTILIZATION:            // 26
        case XPUM_DUMP_MEDIA_ENHANCEMENT_ENGINE_UTILIZATION: // 27
        case XPUM_DUMP_COMPUTE_ENGINE_GROUP_UTILIZATION:   // 31
        case XPUM_DUMP_RENDER_ENGINE_GROUP_UTILIZATION:    // 32
        case XPUM_DUMP_MEDIA_ENGINE_GROUP_UTILIZATION:     // 33
        case XPUM_DUMP_COPY_ENGINE_GROUP_UTILIZATION:      // 34
            return true;
        default:
            return false;
    }
}

// Get metric name for display
static const char* getMetricName(int metricId) {
    switch (metricId) {
        case XPUM_DUMP_GPU_UTILIZATION: return "GPU_UTILIZATION";
        case XPUM_DUMP_GPU_CORE_TEMPERATURE: return "GPU_CORE_TEMPERATURE";
        case XPUM_DUMP_MEMORY_TEMPERATURE: return "MEMORY_TEMPERATURE";
        case XPUM_DUMP_MEMORY_READ_THROUGHPUT: return "MEMORY_READ_THROUGHPUT";
        case XPUM_DUMP_MEMORY_WRITE_THROUGHPUT: return "MEMORY_WRITE_THROUGHPUT";
        case XPUM_DUMP_EU_ACTIVE: return "EU_ACTIVE";
        case XPUM_DUMP_EU_STALL: return "EU_STALL";
        case XPUM_DUMP_EU_IDLE: return "EU_IDLE";
        case XPUM_DUMP_MEMORY_BANDWIDTH: return "MEMORY_BANDWIDTH";
        case XPUM_DUMP_COMPUTE_ENGINE_UTILIZATION: return "COMPUTE_ENGINE_UTILIZATION";
        case XPUM_DUMP_RENDER_ENGINE_UTILIZATION: return "RENDER_ENGINE_UTILIZATION";
        case XPUM_DUMP_DECODE_ENGINE_UTILIZATION: return "DECODE_ENGINE_UTILIZATION";
        case XPUM_DUMP_ENCODE_ENGINE_UTILIZATION: return "ENCODE_ENGINE_UTILIZATION";
        case XPUM_DUMP_COPY_ENGINE_UTILIZATION: return "COPY_ENGINE_UTILIZATION";
        case XPUM_DUMP_MEDIA_ENHANCEMENT_ENGINE_UTILIZATION: return "MEDIA_ENHANCEMENT_ENGINE_UTILIZATION";
        case XPUM_DUMP_COMPUTE_ENGINE_GROUP_UTILIZATION: return "COMPUTE_ENGINE_GROUP_UTILIZATION";
        case XPUM_DUMP_RENDER_ENGINE_GROUP_UTILIZATION: return "RENDER_ENGINE_GROUP_UTILIZATION";
        case XPUM_DUMP_MEDIA_ENGINE_GROUP_UTILIZATION: return "MEDIA_ENGINE_GROUP_UTILIZATION";
        case XPUM_DUMP_COPY_ENGINE_GROUP_UTILIZATION: return "COPY_ENGINE_GROUP_UTILIZATION";
        default: return "UNKNOWN";
    }
}

// Check if MEI devices are accessible for telemetry
static bool checkMeiDevicePermissions() {
    DIR* dir = opendir("/dev");
    if (dir == nullptr) {
        std::cerr << "Warning: Unable to access /dev directory for MEI device permission check" << std::endl;
        return true; // Can't check, proceed without suppression
    }
    
    struct dirent* entry;
    bool meiDeviceFound = false;
    bool hasPermissionIssue = false;
    std::vector<std::string> inaccessibleDevices;
    
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        // Check for mei devices (mei0, mei1, etc.)
        if (filename.length() > 3 && filename.substr(0, 3) == "mei" && std::isdigit(filename[3])) {
            meiDeviceFound = true;
            std::string devicePath = "/dev/" + filename;
            
            // Check read access (which is needed for telemetry queries)
            if (access(devicePath.c_str(), R_OK) != 0) {
                hasPermissionIssue = true;
                inaccessibleDevices.push_back(devicePath);
            }
        }
    }
    closedir(dir);
    
    if (hasPermissionIssue && meiDeviceFound) {
        return false;
    }
    
    return true;
}

static std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss (s);
    std::string item;

    while (getline (ss, item, delim)) {
        if (item.size() > 0)
            result.push_back(item);
    }

    return result;
}

static xpum_stats_type_enum statsTypeFromDumpType(xpum_dump_type_t dumpType){
    switch (dumpType) {
    case xpum_dump_type_t::XPUM_DUMP_GPU_UTILIZATION:
        return xpum_stats_type_enum::XPUM_STATS_GPU_UTILIZATION;
    case xpum_dump_type_t::XPUM_DUMP_POWER:
        return xpum_stats_type_enum::XPUM_STATS_POWER;
    case xpum_dump_type_t::XPUM_DUMP_GPU_FREQUENCY:
        return xpum_stats_type_enum::XPUM_STATS_GPU_FREQUENCY;
    case xpum_dump_type_t::XPUM_DUMP_GPU_CORE_TEMPERATURE:
        return xpum_stats_type_enum::XPUM_STATS_GPU_CORE_TEMPERATURE;
    case xpum_dump_type_t::XPUM_DUMP_MEMORY_TEMPERATURE:
        return xpum_stats_type_enum::XPUM_STATS_MEMORY_TEMPERATURE;
    case xpum_dump_type_t::XPUM_DUMP_MEMORY_UTILIZATION:
        return xpum_stats_type_enum::XPUM_STATS_MEMORY_UTILIZATION;
    case xpum_dump_type_t::XPUM_DUMP_MEMORY_READ_THROUGHPUT:
        return xpum_stats_type_enum::XPUM_STATS_MEMORY_READ_THROUGHPUT;
    case xpum_dump_type_t::XPUM_DUMP_MEMORY_WRITE_THROUGHPUT:
        return xpum_stats_type_enum::XPUM_STATS_MEMORY_WRITE_THROUGHPUT;
    case xpum_dump_type_t::XPUM_DUMP_ENERGY:
        return xpum_stats_type_enum::XPUM_STATS_ENERGY;
    case xpum_dump_type_t::XPUM_DUMP_EU_ACTIVE:
        return xpum_stats_type_enum::XPUM_STATS_EU_ACTIVE;
    case xpum_dump_type_t::XPUM_DUMP_EU_STALL:
        return xpum_stats_type_enum::XPUM_STATS_EU_STALL;
    case xpum_dump_type_t::XPUM_DUMP_EU_IDLE:
        return xpum_stats_type_enum::XPUM_STATS_EU_IDLE;
    case xpum_dump_type_t::XPUM_DUMP_RAS_ERROR_CAT_RESET:
        return xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_RESET;
    case xpum_dump_type_t::XPUM_DUMP_RAS_ERROR_CAT_PROGRAMMING_ERRORS:
        return xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS;
    case xpum_dump_type_t::XPUM_DUMP_RAS_ERROR_CAT_DRIVER_ERRORS:
        return xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS;
    case xpum_dump_type_t::XPUM_DUMP_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE:
        return xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE;
    case xpum_dump_type_t::XPUM_DUMP_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE:
        return xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE;
    case xpum_dump_type_t::XPUM_DUMP_MEMORY_BANDWIDTH:
        return xpum_stats_type_enum::XPUM_STATS_MEMORY_BANDWIDTH;
    case xpum_dump_type_t::XPUM_DUMP_MEMORY_USED:
        return xpum_stats_type_enum::XPUM_STATS_MEMORY_USED;
    case xpum_dump_type_t::XPUM_DUMP_PCIE_READ_THROUGHPUT:
        return xpum_stats_type_enum::XPUM_STATS_PCIE_READ_THROUGHPUT;
    case xpum_dump_type_t::XPUM_DUMP_PCIE_WRITE_THROUGHPUT:
        return xpum_stats_type_enum::XPUM_STATS_PCIE_WRITE_THROUGHPUT;
    case xpum_dump_type_t::XPUM_DUMP_COMPUTE_XE_LINK_THROUGHPUT:
        return xpum_stats_type_enum::XPUM_STATS_FABRIC_THROUGHPUT;
    case xpum_dump_type_t::XPUM_DUMP_COMPUTE_ENGINE_UTILIZATION:
        return xpum_stats_type_enum::XPUM_STATS_ENGINE_UTILIZATION;
    case xpum_dump_type_t::XPUM_DUMP_RENDER_ENGINE_UTILIZATION:
        return xpum_stats_type_enum::XPUM_STATS_ENGINE_UTILIZATION;
    case xpum_dump_type_t::XPUM_DUMP_DECODE_ENGINE_UTILIZATION:
        return xpum_stats_type_enum::XPUM_STATS_ENGINE_UTILIZATION;
    case xpum_dump_type_t::XPUM_DUMP_ENCODE_ENGINE_UTILIZATION:
        return xpum_stats_type_enum::XPUM_STATS_ENGINE_UTILIZATION;
    case xpum_dump_type_t::XPUM_DUMP_COPY_ENGINE_UTILIZATION:
        return xpum_stats_type_enum::XPUM_STATS_ENGINE_UTILIZATION;
    case xpum_dump_type_t::XPUM_DUMP_MEDIA_ENHANCEMENT_ENGINE_UTILIZATION:
        return xpum_stats_type_enum::XPUM_STATS_ENGINE_UTILIZATION;
    case xpum_dump_type_t::XPUM_DUMP_3D_ENGINE_UTILIZATION:
        return xpum_stats_type_enum::XPUM_STATS_ENGINE_UTILIZATION;
    case xpum_dump_type_t::XPUM_DUMP_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE:
        return xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE;
    case xpum_dump_type_t::XPUM_DUMP_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE:
        return xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE;
    case xpum_dump_type_t::XPUM_DUMP_COMPUTE_ENGINE_GROUP_UTILIZATION:
        return xpum_stats_type_enum::XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION;
    case xpum_dump_type_t::XPUM_DUMP_RENDER_ENGINE_GROUP_UTILIZATION:
        return xpum_stats_type_enum::XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION;
    case xpum_dump_type_t::XPUM_DUMP_MEDIA_ENGINE_GROUP_UTILIZATION:
        return xpum_stats_type_enum::XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION;
    case xpum_dump_type_t::XPUM_DUMP_COPY_ENGINE_GROUP_UTILIZATION:
        return xpum_stats_type_enum::XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION;
    case xpum_dump_type_t::XPUM_DUMP_FREQUENCY_THROTTLE_REASON_GPU:
        return xpum_stats_type_enum::XPUM_STATS_FREQUENCY_THROTTLE_REASON_GPU;
    case xpum_dump_type_t::XPUM_DUMP_MEDIA_ENGINE_FREQUENCY:
        return xpum_stats_type_enum::XPUM_STATS_MEDIA_ENGINE_FREQUENCY;

    default:
        return xpum_stats_type_enum::XPUM_STATS_MAX;
    }
}

std::string ComletDump::getEnv(){
    std::string env = "";
    std::vector<int> statsTypes;
    for (auto id : this->opts->metricsIdList) {
        int statsType = statsTypeFromDumpType((xpum_dump_type_t)id);
        if(std::find(statsTypes.begin(), statsTypes.end(), statsType) == statsTypes.end()){
            statsTypes.push_back(statsType);
        }
    }
    std::sort(statsTypes.begin(), statsTypes.end());

    for(auto stats : statsTypes){
        if(!env.empty()){
            env += ",";
        }
        env += std::to_string(stats);
    }
    return env;
}

// For PVC idle power
static std::map<int, std::string> gpu_id_to_bdfs;
static std::map<std::string, int> gpu_bdf_to_ids;
static std::map<std::string, int> gpu_bdf_to_tile_num;
bool ComletDump::dumpIdlePowerOnly() { 
    std::set<std::string> gpu_bdfs;
    DIR *pdir = NULL;
    struct dirent *pdirent = NULL;
    pdir = opendir("/sys/class/drm");
    if (pdir != NULL) {
        while ((pdirent = readdir(pdir)) != NULL) {
            if (pdirent->d_name[0] == '.') {
                continue;
            }
            if (strncmp(pdirent->d_name, "render", 6) == 0) {
                continue;
            }
            if (strncmp(pdirent->d_name, "card", 4) != 0) {
                continue;
            }
            if (strstr(pdirent->d_name, "-") != NULL) {
                continue;
            }
            UEvent uevent;
            if (getUEvent(uevent, pdirent->d_name) == true) {
                if (uevent.pciId.compare(0, 3, "0BD") == 0 ||
                    uevent.pciId.compare(0, 3, "0BE") == 0 ||
                    uevent.pciId.compare(0, 3, "0B6") == 0) {
                    gpu_bdfs.insert(uevent.bdf);
                    if (uevent.pciId.compare("0BD9") == 0 ||
                        uevent.pciId.compare("0BDA") == 0 ||
                        uevent.pciId.compare("0BDB") == 0 ||
                        uevent.pciId.compare("0B6E") == 0) {
                        gpu_bdf_to_tile_num[uevent.bdf] = 1;
                    } else {
                        gpu_bdf_to_tile_num[uevent.bdf] = 2;
                    }
                }
            }
        }
        closedir(pdir);
    }
    if (gpu_bdfs.size() == 0)
        return false;
    int id = 0;
    for(auto& gpu_bdf : gpu_bdfs) {
        gpu_id_to_bdfs[id] = gpu_bdf;
        gpu_bdf_to_ids[gpu_bdf] = id;
        id++;
    }
    if (this->opts->metricsIdList.size() == 1 && this->opts->metricsIdList[0] == XPUM_DUMP_POWER)
        return true;
    return false;
}

void ComletDump::setupOptions() {
    this->opts = std::unique_ptr<ComletDumpOptions>(new ComletDumpOptions());
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceIds, "The device IDs or PCI BDF addresses to query. The value of \"-1\" means all devices.");
    auto tileIdOpt = addOption("-t,--tile", this->opts->deviceTileIds, "The device tile IDs to query. If the device has only one tile, this parameter should not be specified.");

    deviceIdOpt->check([this](const std::string &str) {
        std::string errStr = "Device id should be a non-negative integer or a BDF string. \"-1\" means all devices.";
        std::vector<std::string> deviceIds = split(str, ',');
        for (auto id : deviceIds) {
            if (!isValidDeviceId(id) && !isBDF(id) && (id!="-1")) {
                return errStr;
            }
        }
        return std::string();
    });

    tileIdOpt->check([this](const std::string &str) {
        std::string errStr = "Tile id should be a non-negative integer. \"-1\" means all tiles.";
        std::vector<std::string> tileIds = split(str, ',');
        if(tileIds.size() == 1 && (tileIds[0] =="-1")){
            return std::string();
        }

        for (auto id : tileIds) {
            if (!isValidTileId(id)) {
                return errStr;
            }
        }
        return std::string();
    });
    deviceIdOpt->delimiter(',');
    tileIdOpt->delimiter(',');
    auto metricsListOpt = addOption("-m,--metrics", this->opts->metricsIdList, metricsHelpStr);
    metricsListOpt->delimiter(',');
    metricsListOpt->check(CLI::Range(0, (int)dumpTypeOptions.size() - 1));
    auto timeIntervalOpt = addOption("-i", this->opts->timeInterval, "The interval (in seconds) to dump the device statistics to screen. Default value: 1 second.");
    // timeIntervalOpt->check(CLI::Range(1, std::numeric_limits<int>::max()));
    timeIntervalOpt->check(
        [](const std::string &str) {
            std::string errStr = "Value should be integer larger than or equal to 1 and less than 1000";
            if (!isNumber(str))
                return errStr;
            int value;
            try {
                value = std::stoi(str);
            } catch (const std::out_of_range &oor) {
                return errStr;
            }
            if (value < 1 || value >= 1000)
                return errStr;
            return std::string();
        });
    auto dumpTimesOpt = addOption("-n", this->opts->dumpTimes, "Number of the device statistics dump to screen. The dump will never be ended if this parameter is not specified.\n");
    dumpTimesOpt->check(CLI::Range(1, std::numeric_limits<int>::max()));

#ifdef DAEMONLESS
    std::string msTimeHelp = "";
    auto dumpFileFlag = addOption("--file", this->opts->dumpFilePath, "Dump the raw statistics to the file.");
    auto msTimeIntervalOpt = addOption("--ims", this->opts->msTimeInterval, msTimeHelp +
    "The interval (in milliseconds) to dump the device statistics to file for high-frequency monitoring.\n" +
    "The recommended metrics types for high-frequency sampling: GPU power, GPU frequency, GPU utilization,\n"+
    "GPU temperature, GPU memory read/write/bandwidth, GPU PCIe read/write, GPU engine utilizations, Xe Link throughput.");
    msTimeIntervalOpt->check(
        [](const std::string &str) {
            std::string errStr = "Value should be integer larger than or equal to 10 and less than or equal 1000";
            if (!isNumber(str))
                return errStr;
            int value;
            try {
                value = std::stoi(str);
            } catch (const std::out_of_range &oor) {
                return errStr;
            }
            if (value < 10 || value > 1000)
                return errStr;
            return std::string();
        });
    msTimeIntervalOpt->excludes(timeIntervalOpt);
    msTimeIntervalOpt->excludes(dumpTimesOpt);
    msTimeIntervalOpt->needs(dumpFileFlag);

    auto dumpTotalTimeFlag = addOption("--time", this->opts->dumpTotalTime, "Dump total time in seconds.");
    dumpTotalTimeFlag->check(
        [](const std::string &str) {
            std::string errStr = "Value should be integer larger than or equal to 0 and less than or equal 100000000";
            if (!isNumber(str))
                return errStr;
            int value;
            try {
                value = std::stoi(str);
            } catch (const std::out_of_range &oor) {
                return errStr;
            }
            if (value < 0 || value > 100000000)
                return errStr;
            return std::string();
        });
    dumpTotalTimeFlag->needs(msTimeIntervalOpt);
#endif

#ifndef DAEMONLESS
    auto dumpRawDataFlag = addFlag("--rawdata", this->opts->rawData, "Dump the required raw statistics to a file in background.");
    auto startDumpFlag = addFlag("--start", this->opts->startDumpTask, "Start a new background task to dump the raw statistics to a file. The task ID and the generated file path are returned.");
    auto stopDumpOpt = addOption("--stop", this->opts->dumpTaskId, "Stop one active dump task.");
    auto listDumpFlag = addFlag("--list", this->opts->listDumpTask, "List all the active dump tasks.");

    dumpRawDataFlag->excludes(timeIntervalOpt);
    dumpRawDataFlag->excludes(dumpTimesOpt);

    startDumpFlag->needs(deviceIdOpt);
    startDumpFlag->needs(metricsListOpt);
    startDumpFlag->needs(dumpRawDataFlag);

    stopDumpOpt->needs(dumpRawDataFlag);
    stopDumpOpt->excludes(deviceIdOpt);
    stopDumpOpt->excludes(tileIdOpt);
    stopDumpOpt->excludes(metricsListOpt);
    stopDumpOpt->excludes(timeIntervalOpt);
    stopDumpOpt->excludes(dumpTimesOpt);

    listDumpFlag->needs(dumpRawDataFlag);
    listDumpFlag->excludes(deviceIdOpt);
    listDumpFlag->excludes(tileIdOpt);
    listDumpFlag->excludes(metricsListOpt);
    listDumpFlag->excludes(timeIntervalOpt);
    listDumpFlag->excludes(dumpTimesOpt);
#endif
    addFlag("--date", this->opts->showDate, "Show date in timestamp.");
}

std::unique_ptr<nlohmann::json> ComletDump::run() {
    std::unique_ptr<nlohmann::json> json;

    // Check MEI device permissions once at the start
    if (!meiPermissionChecked) {
        meiPermissionChecked = true;
        std::vector<int> requestedMeiMetrics;
        
        for (auto metric : this->opts->metricsIdList) {
            if (requiresMeiAccess(metric)) {
                requestedMeiMetrics.push_back(metric);
            }
        }
        
        // Check MEI device permissions only if MEI-dependent metrics are requested
        if (!requestedMeiMetrics.empty()) {
            if (!checkMeiDevicePermissions()) {
                hasMeiPermission = false;
                std::cerr << "\nWARNING: Elevated privileges required for the following " 
                          << (requestedMeiMetrics.size() == 1 ? "metric" : "metrics") << ":\n";
                for (auto metric : requestedMeiMetrics) {
                    suppressedMeiMetrics.insert(metric);
                    std::cerr << "  - Metric " << metric << " (" << getMetricName(metric) << ") - will display as N/A\n";
                }
                std::cerr << "\n" << (requestedMeiMetrics.size() == 1 ? "This metric requires" : "These metrics require")
                          << " access to MEI devices for GPU telemetry.\n";
                std::cerr << "Please run with elevated privileges (sudo) to collect " 
                          << (requestedMeiMetrics.size() == 1 ? "this metric" : "these metrics") << ".\n" << std::endl;
            }
        }
    }

    // check metrics is unique
    if (std::adjacent_find(this->opts->metricsIdList.begin(), this->opts->metricsIdList.end()) != this->opts->metricsIdList.end()) {
        json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
        (*json)["error"] = "Duplicated metrics type";
        return json;
    }

    // In this case the ims is set
    if(this->opts->msTimeInterval != 0){
        int64_t interval = this->opts->msTimeInterval / 2;
        // monitor freq set is {5, 10, 20, 50, 100, 200, 500, 1000}
        if (interval >= 500){
            interval = 500;
        } else if (interval >= 200) {
            interval = 200;
        } else if (interval >= 100) {
            interval = 100;
        } else if (interval >= 50) {
            interval = 50;
        } else if (interval >= 20) {
            interval = 20;
        } else if (interval >= 10) {
            interval = 10;
        } else {
            interval = 5;
        }
        this->coreStub->setAgentConfig("XPUM_AGENT_CONFIG_SAMPLE_INTERVAL", &interval);
    }

    if (this->opts->rawData) {
        if (this->opts->startDumpTask && !this->opts->deviceIds.empty()) {
            if (this->opts->deviceIds.size() > 1) {
                json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
                (*json)["error"] = "Dumping to file is not supported for multiple devices";
            } if(this->opts->deviceTileIds.size() > 1){
                json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
                (*json)["error"] = "Dumping to file is not supported for multiple tiles";
            }else {
                int deviceId = std::stoi(this->opts->deviceIds[0]);
                int tileId = std::stoi(this->opts->deviceTileIds[0]);
                std::vector<xpum_dump_type_t> dumpTypeList;
                for (auto i : this->opts->metricsIdList) {
                    auto &m = dumpTypeOptions[i];
                    dumpTypeList.push_back(m.dumpType);
                }
                json = this->coreStub->startDumpRawDataTask(deviceId, tileId, dumpTypeList, this->opts->showDate);
            }
        } else if (this->opts->listDumpTask) {
            json = this->coreStub->listDumpRawDataTasks();
        } else if (this->opts->dumpTaskId != -1) {
            json = this->coreStub->stopDumpRawDataTask(this->opts->dumpTaskId);
        } else {
            json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
            (*json)["error"] = "Unknow operation";
        }
#ifdef DAEMONLESS
    } else if (gpu_id_to_bdfs.size() > 0 && this->opts->metricsIdList.size() == 1 && this->opts->metricsIdList[0] == XPUM_DUMP_POWER) {
        json = getMetricsFromSysfs();
#endif
    } else {
        json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
        for (auto deviceId : this->opts->deviceIds) {
            int targetId = -1;
            if (isNumber(deviceId)) {
                targetId = std::stoi(deviceId);
            } else {
                auto convertResult = this->coreStub->getDeivceIdByBDF(deviceId.c_str(), &targetId);
                if (convertResult->contains("error")) {
                    return convertResult;
                }
            }
            auto tempdeviceJsons = this->coreStub->getStatistics(targetId);
            deviceJsons[deviceId] = combineTileAndDeviceLevel(*tempdeviceJsons);
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> ComletDump::combineTileAndDeviceLevel(nlohmann::json rawJson){
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    *json = rawJson;
    if (!json->contains("tile_level"))
        return json;
    if (!json->contains("device_level"))
        rawJson["device_level"] = {};
    std::vector<nlohmann::json> deviceLevelStatsDataList = rawJson["device_level"];
    std::vector<nlohmann::json> tileLevelStatsDataList = rawJson["tile_level"];
    std::set<std::string> deviceMetrics;
    for (auto deviceLevelStatsData: deviceLevelStatsDataList) 
        deviceMetrics.insert(deviceLevelStatsData["metrics_type"].get<std::string>());
    std::set<std::string> tileMetrics;
    for (auto tileLevelStatsData: tileLevelStatsDataList) {
        nlohmann::json StatsData = tileLevelStatsData["data_list"];
        for (auto data: StatsData) 
            tileMetrics.insert(data["metrics_type"].get<std::string>());
    }
    std::set<std::string> metricsList;
    std::set_difference( tileMetrics.begin(), tileMetrics.end(), deviceMetrics.begin(), deviceMetrics.end(), std::inserter(metricsList, metricsList.begin()));
    nlohmann::json tmpJson;
    for (auto metric: metricsList){
        int c = 0; // count of current tiles which contain the certain metric
        for (auto tileLevelStatsData: tileLevelStatsDataList) {
            nlohmann::json StatsData = tileLevelStatsData["data_list"];
            for (std::size_t i = 0; i < StatsData.size(); i++) {
                if(metric == StatsData[i]["metrics_type"]){
                    std::string met = "value";
                    if (StatsData[i].contains("avg"))
                        met = "avg";
                    if ( c == 0 ) {
                        tmpJson[metric]["metrics_type"] = StatsData[i]["metrics_type"];
                        tmpJson[metric][met] = StatsData[i][met];
                    }
                    else{
                        if (sumMetricsList.find(metric) != sumMetricsList.end()){
                            if(StatsData[i][met].is_number_float())
                                tmpJson[metric][met] = tmpJson[metric][met].get<double>() + StatsData[i][met].get<double>();
                            else
                                tmpJson[metric][met] = tmpJson[metric][met].get<uint64_t>() + StatsData[i][met].get<uint64_t>();
                        }
                        else{
                            auto doubleNumber = (tmpJson[metric][met].get<double>() * c + StatsData[i][met].get<double>())/ (double)( c + 1 );
                            tmpJson[metric][met] = round(doubleNumber * 100) /100;
                        }
                    }
                    c += 1;
                }
            }
        }
    }
    for (auto& item : tmpJson.items())
        (*json)["device_level"].push_back(item.value());
    //engine metrics
    if (!json->contains("engine_util")) {
        (*json)["engine_util"]={};
        for (auto tileLevelStatsData: tileLevelStatsDataList) {
            int t = tileLevelStatsData["tile_id"].get<int>();
            if (tileLevelStatsData.contains("engine_util")){
                (*json)["engine_util"]["tile_id_"+std::to_string(t)] = tileLevelStatsData["engine_util"];
            }
        }
    }
    return json;
}

void ComletDump::getJsonResult(std::ostream &out, bool raw) {
    if (!this->opts->rawData) {
        out << "Not supported without the --rawdata option, which is only available in the daemon version" << std::endl;
        return;
    } else {
        ComletBase::getJsonResult(out, raw);
    }
};

void ComletDump::dumpRawDataToFile(std::ostream &out) {
    auto json = run();
    if (json->contains("error")) {
        out << "Error: " << json->value("error", "") << std::endl;
        return;
    }
    if (this->opts->startDumpTask) {
        if (!json->contains("task_id") || !json->contains("dump_file_path")) {
            out << "Error occurs" << std::endl;
            return;
        }
        out << "Task " << (*json)["task_id"] << " is started." << std::endl;
        out << "Dump file path: " << json->value("dump_file_path", "") << std::endl;
    } else if (this->opts->listDumpTask) {
        for (auto taskId : (*json)["dump_task_ids"]) {
            out << "Task " << taskId.get<int>() << " is running." << std::endl;
        }
    } else if (this->opts->dumpTaskId != -1) {
        if (!json->contains("task_id") || !json->contains("dump_file_path")) {
            out << "Error occurs" << std::endl;
            return;
        }
        out << "Task " << (*json)["task_id"] << " is stopped." << std::endl;
        out << "Dump file path: " << json->value("dump_file_path", "") << std::endl;
    }
}

void ComletDump::waitForEsc() {
    int key;
    std::cout << "Dump data to file " << this->opts->dumpFilePath << ". Press the key ESC to stop dumping." << std::endl;
    while (true) {
        key = getChar();
        if (key == -1) {
            std::cerr << "Something wrong in getChar." << std::endl;
            keepDumping = false;
            break;
        }
        if (key == 27) {
            keepDumping = false;
            break;
        }
    }
}

void ComletDump::getTableResult(std::ostream &out) {
    if (this->opts->rawData) {
        dumpRawDataToFile(out);
    } else {
#ifdef DAEMONLESS
        keepDumping = true;
        if (!this->opts->dumpFilePath.empty()){
            dumpFile.open(this->opts->dumpFilePath);
            if (!dumpFile) {
                std::cout << "Error: "
                        << "open file failed" << std::endl;
                return;
            }
            std::thread([this] { this->waitForEsc(); }).detach();
            printByLine(dumpFile);
            dumpFile.close();
            std::cout << "Dumping is stopped." << std::endl;
        } else if (gpu_id_to_bdfs.size() > 0 && this->opts->metricsIdList.size() == 1 && this->opts->metricsIdList[0] == XPUM_DUMP_POWER) {
            printByLineWithoutInitializeCore(out);
        } else {
            printByLine(out);
        }
#else
        printByLine(out); 
#endif
    }
}

typedef std::function<std::string()> getValueFunc;

struct DumpColumn {
    std::string header;
    getValueFunc getValue;

    DumpColumn(
        std::string header,
        getValueFunc getValue)
        : header(header),
          getValue(getValue) {}
};

std::string keepTwoDecimalPrecision(double value) {
    std::ostringstream os;
    os << std::fixed;
    os << std::setprecision(2);
    os << value;
    return os.str();
}

std::string getJsonValue(nlohmann::json obj, int scale) {
    if (obj.is_null()) {
        return "";
    }
    if (obj.is_number_float()) {
        auto value = obj.get<double>();
        value /= scale;
        return keepTwoDecimalPrecision(value);
    } else {
        auto value = obj.get<int64_t>();
        if (scale != 1) {
            return keepTwoDecimalPrecision(value / (double)scale);
        } else {
            return std::to_string(value);
        }
    }
}

std::unique_ptr<nlohmann::json> ComletDump::getMetricsFromSysfs() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    std::vector<std::string> bdfs;
    for (auto& gpu_bdf_to_id : gpu_bdf_to_ids) {
        bdfs.push_back(gpu_bdf_to_id.first);
    }

    std::vector<std::unique_ptr<nlohmann::json>> datas = this->coreStub->getMetricsFromSysfs(bdfs);
    if (datas.size() == 0) {
        (*json)["error"] = "Device not found";
        (*json)["errno"] = XPUM_CLI_ERROR_DEVICE_NOT_FOUND;
        return json;
    }

    std::set<int> visited;
    for (auto deviceId : this->opts->deviceIds) {
        int targetId = -1;
        if (isNumber(deviceId)) {
            targetId = std::stoi(deviceId);
        } else {
            targetId = gpu_bdf_to_ids[deviceId];
        }
        if (visited.count(targetId) == 0) {
            if (targetId >= 0 && targetId < (int)datas.size()) {
                deviceJsons[deviceId] = std::move(datas[targetId]);
            }
        } else {
            auto data = this->coreStub->getMetricsFromSysfs({gpu_id_to_bdfs[targetId]});
            if (!data.empty())
                deviceJsons[deviceId] = std::move(data.front());
        }
        visited.insert(targetId);
    }
    return json;
}

void ComletDump::printByLineWithoutInitializeCore(std::ostream &out) {
    // out << std::left << std::setw(25) << std::setfill(' ') << "Timestamp, ";

    if (this->opts->deviceIds.size() == 0) {
        out << "Device id should be provided" << std::endl;
        exit_code = XPUM_CLI_ERROR_BAD_ARGUMENT;
        return;
    }
    if (this->opts->metricsIdList.size() == 0) {
        out << "Metrics types should be provided" << std::endl;
        exit_code = XPUM_CLI_ERROR_BAD_ARGUMENT;
        return;
    }

    // convert deviceIds if deviceId equals -1
    if(this->opts->deviceIds.size()==1 && this->opts->deviceIds[0]=="-1"){
        std::vector<std::string> deviceIds;
        for (auto& gpu_id_to_bdf : gpu_id_to_bdfs)
            deviceIds.push_back(std::to_string(gpu_id_to_bdf.first));
        this->opts->deviceIds = deviceIds;
    }

    for (auto deviceIdStr : this->opts->deviceIds) {
        if (gpu_id_to_bdfs.count(std::stoi(deviceIdStr)) == 0 && gpu_bdf_to_ids.count(deviceIdStr) == 0) {
            out << "Error: Device not found" << std::endl;
            exit_code = XPUM_CLI_ERROR_DEVICE_NOT_FOUND;
            return;
        }
    }

    // check deviceId and tileId is valid
    for (auto deviceId : this->opts->deviceIds) {
        std::string deviceBDF = deviceId;
        if (isNumber(deviceId)) {
            deviceBDF = gpu_id_to_bdfs[std::stoi(deviceId)];
        }
        if (!(this->opts->deviceTileIds.size() == 1 && this->opts->deviceTileIds[0] == "-1")) {
            for(auto tile : this->opts->deviceTileIds){
                if (std::stoi(tile) >= gpu_bdf_to_tile_num[deviceBDF]) {
                    out << "Error: Tile not found" << std::endl;
                    exit_code = XPUM_CLI_ERROR_TILE_NOT_FOUND;
                    return;
                }
            }
        }
    }

    if (this->opts->deviceIds.size() > 1) {
        std::vector<std::string> ids = this->opts->deviceIds;
        sort(ids.begin(),ids.end());
        if (std::adjacent_find(ids.begin(), ids.end()) != ids.end()) {
            out << "Error: Duplicated device ids" << std::endl;
            return;
        }
    }

    // try run
    auto res = run();

    // construct column schema
    std::vector<DumpColumn> columnSchemaList;

    // timestamp column
    columnSchemaList.push_back({"Timestamp",
                                [this]() {
                                    if (this->opts->showDate)
                                        return CoreStub::isotimestamp(time(nullptr) * 1000);
                                    else
                                        return CoreStub::isotimestamp(time(nullptr) * 1000, true);
                                }});

    // device id column
    columnSchemaList.push_back({"DeviceId",
                                [this]() {
                                    return this->curDeviceId;
                                }});

    // tile id
    if (!(this->opts->deviceTileIds.size() == 1 && this->opts->deviceTileIds[0] == "-1")) {
        columnSchemaList.push_back({"TileId",
                                    [this]() {
                                        return this->curTileId;
                                    }});
    }

    // other columns
    for (std::size_t i = 0; i < this->opts->metricsIdList.size(); i++) {
        int metric = this->opts->metricsIdList[i];
        auto config = dumpTypeOptions[metric];
        if (config.optionType == xpum::dump::DUMP_OPTION_STATS) {
            DumpColumn dc{
                std::string(config.name),
                [config, this, metric]() {
                    // Check if this metric is suppressed due to MEI permission
                    if (this->suppressedMeiMetrics.count(metric) > 0) {
                        return std::string("N/A");
                    }
                    
                    std::string metricKey = config.key;
                    if (statsJson != nullptr) {
                        for (auto metricObj : (*statsJson)) {
                            if (metricObj["metrics_type"].get<std::string>().compare(metricKey) == 0) {
                                if (metricObj.contains("avg")) {
                                    return getJsonValue(metricObj["avg"], config.scale);
                                } else {
                                    // counter type
                                    return getJsonValue(metricObj["value"], config.scale);
                                }
                            }
                        }
                    }
                    return std::string();
                }};
            columnSchemaList.push_back(dc);
        }
    }

    // print table header
    for (std::size_t i = 0; i < columnSchemaList.size(); i++) {
        auto dc = columnSchemaList[i];
        out << dc.header;
        if (i < columnSchemaList.size() - 1) {
            out << ", ";
        }
    }

    out << std::endl;

    int iter = 0;

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(this->opts->timeInterval * 900));
        res = run();
        if (res->contains("error")) {
            out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
            return;
        }
        for (auto deviceId : this->opts->deviceIds) {
            curDeviceId = deviceId;
            statsJson = nullptr;

            for(auto tileId : this->opts->deviceTileIds){
                curTileId = tileId;
                if (this->opts->deviceTileIds.size() == 1 && this->opts->deviceTileIds[0] == "-1") {
                    if (deviceJsons[deviceId]->contains("device_level")) {
                        statsJson = std::make_shared<nlohmann::json>((*deviceJsons[deviceId])["device_level"]);
                    }
                } else {
                    if (deviceJsons[deviceId]->contains("tile_level")) {
                        auto tiles = (*deviceJsons[deviceId])["tile_level"].get<std::vector<nlohmann::json>>();
                        for (auto tile : tiles) {
                            if (tile.contains("tile_id") && tile["tile_id"].get<int>() == std::stoi(tileId) && tile.contains("data_list")) {
                                statsJson = std::make_shared<nlohmann::json>(tile["data_list"]);
                                break;
                            }
                        }
                    }
                }

                for (std::size_t i = 0; i < columnSchemaList.size(); i++) {
                    auto dc = columnSchemaList[i];
                    auto value = dc.getValue();
                    if (value.empty())
                        value = "N/A";
                    if (value.size() < 4) {
                        out << std::string(4 - value.size(), ' ');
                    }
                    out << value;
                    if (i < columnSchemaList.size() - 1) {
                        out << ", ";
                    }
                }
                out << std::endl;
            }
        }
        if (this->opts->dumpTimes != -1 && ++iter >= this->opts->dumpTimes) {
            break;
        }
    }
}

void ComletDump::printByLine(std::ostream &out) {
    // out << std::left << std::setw(25) << std::setfill(' ') << "Timestamp, ";

    if (this->opts->deviceIds.size() == 0) {
        out << "Device id should be provided" << std::endl;
        exit_code = XPUM_CLI_ERROR_BAD_ARGUMENT;
        return;
    }
    if (this->opts->metricsIdList.size() == 0) {
        out << "Metrics types should be provided" << std::endl;
        exit_code = XPUM_CLI_ERROR_BAD_ARGUMENT;
        return;
    }

    // convert deviceIds if deviceId equals -1
    if(this->opts->deviceIds.size()==1 && this->opts->deviceIds[0]=="-1"){
        std::vector<std::string> deviceIds;
        auto deviceListJson = this->coreStub->getDeviceList();
        auto deviceList = (*deviceListJson)["device_list"];
        for (auto& device : deviceList) {
            int id = device["device_id"];
            deviceIds.push_back(std::to_string(id));
        }
        this->opts->deviceIds = deviceIds;
    }

    std::string deviceId = this->opts->deviceIds[0];

    // check deviceId and tileId is valid
    for (auto deviceIdStr : this->opts->deviceIds) {
        int deviceId = -1;
        if (!isNumber(deviceIdStr)) {
            auto convertRes = this->coreStub->getDeivceIdByBDF(deviceIdStr.c_str(), &deviceId);
            if (convertRes->contains("error")) {
                out << "Error: " << (*convertRes)["error"].get<std::string>() << std::endl;
                return;
            }
        }
        else
            deviceId = std::stoi(deviceIdStr);
        auto res = this->coreStub->getDeviceProperties(deviceId);
        if (res->contains("error")) {
            out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
            return;
        }
        if (!(this->opts->deviceTileIds.size() == 1 && this->opts->deviceTileIds[0] == "-1")) {
            std::stringstream number_of_tiles((*res)["number_of_tiles"].get<std::string>());
            int num_tiles = 0;
            number_of_tiles >> num_tiles;

            for(auto tile : this->opts->deviceTileIds){
                if (std::stoi(tile) >= num_tiles) {
                    out << "Error: Tile not found" << std::endl;
                    return;
                }
            }
        }
    }

    if (this->opts->deviceIds.size() > 1) {
        std::vector<std::string> ids = this->opts->deviceIds;
        sort(ids.begin(),ids.end());
        if (std::adjacent_find(ids.begin(), ids.end()) != ids.end()) {
            out << "Error: Duplicated device ids" << std::endl;
            return;
        }
        bool hasPerEngineMetrics = false;
        for (auto metricId: this->opts->metricsIdList) {
            if (metricId == 21) {
                out << "Error: Xe Link throughput is not supported by multiple devices" << std::endl;
                return;
            }
            if (metricId >= 22 && metricId <= 28) {
                hasPerEngineMetrics = true;
                break;
            }
        }
        if (hasPerEngineMetrics) {
            bool sameDeviceModel = true;
            std::string deviceName;
            for (auto deviceIdStr : this->opts->deviceIds) {
                int targetId = -1;
                if (isNumber(deviceIdStr)) {
                    targetId = std::stoi(deviceIdStr);
                } else {
                    auto convertResult = this->coreStub->getDeivceIdByBDF(deviceIdStr.c_str(), &targetId);
                    if (convertResult->contains("error")) {
                        out << "Error: " << (*convertResult)["error"].get<std::string>() << std::endl;
                        return;
                    }
                }
                auto res = this->coreStub->getDeviceProperties(targetId);
                if (deviceName.empty()) {
                    deviceName = (*res)["device_name"].get<std::string>();
                } else {
                    if (deviceName != (*res)["device_name"].get<std::string>()) {
                        sameDeviceModel = false;
                        break;
                    }
                }
            }
            if (!sameDeviceModel) {
                out << "Error: For per-engine utilization, the device models should be the same" << std::endl;
                return;
            }
        }
    }

    // try run
    auto res = run();

    int targetId = -1;
    if (isNumber(deviceId)) {
        targetId = std::stoi(deviceId);
    } else {
        auto convertResult = this->coreStub->getDeivceIdByBDF(deviceId.c_str(), &targetId);
        if (convertResult->contains("error")) {
            out << "Error: " << (*convertResult)["error"].get<std::string>() << std::endl;
            return;
        }
    }

    // construct column schema
    std::vector<DumpColumn> columnSchemaList;

    // timestamp column
    columnSchemaList.push_back({"Timestamp",
                                [this]() {
                                    long ms; // Milliseconds
                                    time_t s;  // Seconds
                                    struct timespec spec;
                                    clock_gettime(CLOCK_REALTIME, &spec);

                                    s  = spec.tv_sec;
                                    ms = spec.tv_nsec / 1000000; // Convert nanoseconds to milliseconds
                                    if (this->opts->showDate)
                                        return CoreStub::isotimestamp(s * 1000 + ms);
                                    else
                                        return CoreStub::isotimestamp(s * 1000 + ms, true);
                                }});

    // device id column
    columnSchemaList.push_back({"DeviceId",
                                [this]() {
                                    return this->curDeviceId;
                                }});

    // tile id
    if (!(this->opts->deviceTileIds.size() == 1 && this->opts->deviceTileIds[0] == "-1")) {
        columnSchemaList.push_back({"TileId",
                                    [this]() {
                                        return this->curTileId;
                                    }});
    }

    // other columns
    for (std::size_t i = 0; i < this->opts->metricsIdList.size(); i++) {
        int metric = this->opts->metricsIdList[i];
        auto config = dumpTypeOptions[metric];
        if (config.optionType == xpum::dump::DUMP_OPTION_STATS) {
            DumpColumn dc{
                std::string(config.name),
                [config, this, metric]() {
                    // Check if this metric is suppressed due to MEI permission
                    if (this->suppressedMeiMetrics.count(metric) > 0) {
                        return std::string("N/A");
                    }
                    
                    std::string metricKey = config.key;
                    if (statsJson != nullptr) {
                        for (auto metricObj : (*statsJson)) {
                            if (metricObj["metrics_type"].get<std::string>().compare(metricKey) == 0) {
                                if (metricObj.contains("avg")) {
                                    return getJsonValue(metricObj["avg"], config.scale);
                                } else {
                                    // counter type
                                    return getJsonValue(metricObj["value"], config.scale);
                                }
                            }
                        }
                    }
                    return std::string();
                }};
            columnSchemaList.push_back(dc);
        } else if (config.optionType == xpum::dump::DUMP_OPTION_ENGINE) {
            auto pEngineCountMap = std::make_shared<std::map<int, std::map<int, int>>>();
            pEngineCountMap = this->coreStub->getEngineCount(targetId);

            if (res->contains("error")) {
                out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
                setExitCodeByJson(*res);
                return;
            }

            std::map<int, int> tileIdsMap;
            bool deviceLevelHeader = false;
            for(auto tile : this->opts->deviceTileIds){
                if(pEngineCountMap->find(std::stoi(tile)) != pEngineCountMap->end()){
                    int engineCount = (*pEngineCountMap)[std::stoi(tile)][config.engineType];
                    tileIdsMap.insert({std::stoi(tile),engineCount});
                }
                else if(this->opts->deviceTileIds.size() == 1 && this->opts->deviceTileIds[0] == "-1"){
                    for(auto const &ent1 : (*pEngineCountMap)) {
                        if (ent1.first != -1){
                            int engineCount = (*pEngineCountMap)[ent1.first][config.engineType];
                            tileIdsMap.insert({ent1.first,engineCount});
                        }
                    }
                    deviceLevelHeader = true;
                }
            }

            if (tileIdsMap.size()==0){
                tileIdsMap.insert({-1,0});
                deviceLevelHeader = false;
            }
            for (auto itr = tileIdsMap.begin(); itr != tileIdsMap.end(); ++itr) {
                int tileIdx = -1;
                if (deviceLevelHeader)
                    tileIdx = itr->first;
                int engineCount = itr->second;
                if (engineCount){
                    for (int engineIdx = 0; engineIdx < engineCount; engineIdx++) {
                        std::string header;
                        if (deviceLevelHeader)
                            header = engineNameMap[config.engineType] + " " + std::to_string(tileIdx) + "/" + std::to_string(engineIdx) + " (%)";
                        else
                            header = engineNameMap[config.engineType] + " " + std::to_string(engineIdx) + " (%)";
                        DumpColumn dc{
                            header,
                            [config, tileIdx, engineIdx, this]() {
                                if (engineUtilJson != nullptr) {
                                    nlohmann::json engineUtilByType;
                                    if (tileIdx == -1)
                                        engineUtilByType = (*engineUtilJson)[config.key];
                                    else
                                        engineUtilByType = (*engineUtilJson)["tile_id_"+std::to_string(tileIdx)][config.key];
                                    for (auto u : engineUtilByType) {
                                        if (u["engine_id"].get<int>() == engineIdx) {
                                            #ifndef DAEMONLESS
                                            return getJsonValue(u["avg"], config.scale);
                                            #else
                                            return getJsonValue(u["value"], config.scale);
                                            #endif
                                        }
                                    }
                                }
                                return std::string();
                            }};
                        columnSchemaList.push_back(dc);
                    }
                }
                else{
                    std::string header;
                    if (deviceLevelHeader)
                        header = engineNameMap[config.engineType] + " " + std::to_string(tileIdx) + " (%)";
                    else
                        header = engineNameMap[config.engineType] + " (%)";
                    DumpColumn dc{
                        header,
                        [config, this]() {
                            return std::string();
                        }};
                    columnSchemaList.push_back(dc);
                }
            }
        } else if (config.optionType == xpum::dump::DUMP_OPTION_FABRIC) {
            auto pFabricCountJson = std::shared_ptr<nlohmann::json>();
            pFabricCountJson = this->coreStub->getFabricCount(targetId);

            if (res->contains("error")) {
                out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
                setExitCodeByJson(*res);
                return;
            }

            std::vector<std::string> strTileIds;
            if (this->opts->deviceTileIds.size() == 1 && this->opts->deviceTileIds[0] == "-1"){
                for (auto& el : (*pFabricCountJson).items())
                    strTileIds.push_back(el.key());
            }
            else{
                for(auto tile : this->opts->deviceTileIds){
                    if (pFabricCountJson->contains(tile))
                        strTileIds.push_back(tile);
                }
            }
            for (auto strTileId: strTileIds) {
                for (auto obj : (*pFabricCountJson)[strTileId]) {
                    std::stringstream ss;
                    std::string key;
                    std::string header;
                    // tx
                    ss << deviceId << "/" << obj["tile_id"];
                    ss << "->" << obj["remote_device_id"] << "/" << obj["remote_tile_id"];
                    key = ss.str();
                    header = "XL " + key + " (kB/s)";
                    columnSchemaList.push_back({header,
                                                [config, key, this]() {
                                                    if (fabricThroughputJson != nullptr) {
                                                        for (auto tp : (*fabricThroughputJson)) {
                                                            if (key.compare(tp["name"].get<std::string>())==0) {
                                                                #ifndef DAEMONLESS
                                                                return getJsonValue(tp["avg"], config.scale);
                                                                #else
                                                                return getJsonValue(tp["value"], config.scale);     
                                                                #endif
                                                            }
                                                        }
                                                    }
                                                    return std::string();
                                                }});
                    // rx
                    ss.str("");
                    ss << obj["remote_device_id"] << "/" << obj["remote_tile_id"];
                    ss << "->";
                    ss << deviceId << "/" << obj["tile_id"];
                    key = ss.str();
                    header = "XL " + key + " (kB/s)";
                    columnSchemaList.push_back({header,
                                                [config, key, this]() {
                                                    if (fabricThroughputJson != nullptr) {
                                                        for (auto tp : (*fabricThroughputJson)) {
                                                            if (!key.compare(tp["name"].get<std::string>())) {
                                                                #ifndef DAEMONLESS
                                                                return getJsonValue(tp["avg"], config.scale);
                                                                #else
                                                                return getJsonValue(tp["value"], config.scale);
                                                                #endif
                                                            }
                                                        }
                                                    }
                                                    return std::string();
                                                }});
                }
            }
            if (strTileIds.size() == 0){
                std::string header = "XL (kB/s)"; 
                columnSchemaList.push_back({header,
                            [config, this]() {
                                return std::string();
                            }});
            }
        } else if (config.optionType == xpum::dump::DUMP_OPTION_THROTTLE_REASON) {
            DumpColumn dc{
                std::string(config.name),
                [config, this]() {
                    std::string metricKey = config.key;
                    if (statsJson != nullptr) {
                        for (auto metricObj : (*statsJson)) {
                            if (metricObj["metrics_type"].get<std::string>().compare(metricKey) == 0) {
                                const uint64_t flags = metricObj["value"].get<uint64_t>();
                                std::string ss;
                                if (flags & xpum::dump::ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP) {
                                    ss += "Average Power Excursion | ";
                                }
                                if (flags & xpum::dump::ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP) {
                                    ss += "Burst Power Excursion | ";
                                }
                                if (flags & xpum::dump::ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT) {
                                    ss += "Current Excursion | ";
                                }
                                if (flags & xpum::dump::ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT) {
                                    ss += "Thermal Excursion | ";
                                }
                                if (flags & xpum::dump::ZES_FREQ_THROTTLE_REASON_FLAG_PSU_ALERT) {
                                    ss += "Power Supply Assertion | ";
                                }
                                if (flags & xpum::dump::ZES_FREQ_THROTTLE_REASON_FLAG_SW_RANGE) {
                                    ss += "Software Supplied Frequency Range | ";
                                }
                                if (flags & xpum::dump::ZES_FREQ_THROTTLE_REASON_FLAG_HW_RANGE) {
                                    ss += "Sub Block that has a Lower Frequency | ";
                                }
                                if (flags == 0) {
                                    ss = "Not Throttled | ";
                                }
                                return ss.substr(0, ss.size() - 3);
                            }
                        }
                    }
                    return std::string();
                }};
            columnSchemaList.push_back(dc);
        }
    }

    // print table header
    for (std::size_t i = 0; i < columnSchemaList.size(); i++) {
        auto dc = columnSchemaList[i];
        out << dc.header;
        if (i < columnSchemaList.size() - 1) {
            out << ", ";
        }
    }

    out << std::endl;

    int iter = 0;
    u_int64_t index = 0;
    uint64_t sleepMilliseconds = (this->opts->msTimeInterval == 0) ? (1000 * this->opts->timeInterval) : this->opts->msTimeInterval;

    std::chrono::system_clock::time_point begin = std::chrono::system_clock::now();
    while (keepDumping) {
        if ((this->opts->dumpTotalTime != -1 && (uint64_t)(this->opts->dumpTotalTime) * 1000 <= sleepMilliseconds * index)) {
            keepDumping = false;
            break;
        }

        ++index;

        // for big interval
        if (sleepMilliseconds > 1000){
            auto leftTime = sleepMilliseconds;
            while (leftTime > 1000 && keepDumping){
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                leftTime -= 1000;
            }
            if(!keepDumping){
                break;
            }
        }

        std::this_thread::sleep_until(begin + std::chrono::milliseconds(sleepMilliseconds * index));
        res = run();
        if (res->contains("error")) {
            out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
            return;
        }
        for (auto deviceId : this->opts->deviceIds) {
            curDeviceId = deviceId;
            statsJson = nullptr;
            engineUtilJson = nullptr;
            fabricThroughputJson = std::make_shared<nlohmann::json>((*deviceJsons[deviceId])["fabric_throughput"]);

            for(auto tile : this->opts->deviceTileIds){
                curTileId = tile;
                if (this->opts->deviceTileIds.size() == 1 && this->opts->deviceTileIds[0] == "-1") {
                    if (deviceJsons[deviceId]->contains("device_level")) {
                        statsJson = std::make_shared<nlohmann::json>((*deviceJsons[deviceId])["device_level"]);
                    }
                    if (deviceJsons[deviceId]->contains("engine_util")) {
                        engineUtilJson = std::make_shared<nlohmann::json>((*deviceJsons[deviceId])["engine_util"]);
                    }
                } else {
                    if (deviceJsons[deviceId]->contains("tile_level")) {
                        auto tiles = (*deviceJsons[deviceId])["tile_level"].get<std::vector<nlohmann::json>>();
                        for (auto tile : tiles) {
                            if (tile.contains("tile_id") && tile["tile_id"].get<int>() == std::stoi(curTileId) && tile.contains("data_list")) {
                                statsJson = std::make_shared<nlohmann::json>(tile["data_list"]);
                                if (tile.contains("engine_util")) {
                                    engineUtilJson = std::make_shared<nlohmann::json>(tile["engine_util"]);
                                }
                                break;
                            }
                        }
                    }
                }

                for (std::size_t i = 0; i < columnSchemaList.size(); i++) {
                    auto dc = columnSchemaList[i];
                    auto value = dc.getValue();
                    if (value.empty())
                        value = "N/A";
                    if (value.size() < 4) {
                        out << std::string(4 - value.size(), ' ');
                    }
                    out << value;
                    if (i < columnSchemaList.size() - 1) {
                        out << ", ";
                    }
                }
                out << std::endl;
            }
        }
        if ((this->opts->dumpTimes != -1 && ++iter >= this->opts->dumpTimes)) {
            keepDumping = false;
        }

    }
}

} // end namespace xpum::cli
