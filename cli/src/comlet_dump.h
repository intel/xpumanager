/* 
 *  Copyright (C) 2021-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_dump.h
 */

#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"
#include "internal_dump_raw_data.h"
#include "xpum_structs.h"
#include <dirent.h>
#include <fcntl.h>
#include <dlfcn.h>

using xpum::dump::dumpTypeOptions;

namespace xpum::cli {

struct ComletDumpOptions {
    std::vector<std::string> deviceIds = {"-1"};
    std::vector<std::string> deviceTileIds = {"-1"};
    std::vector<int> metricsIdList;
    uint32_t timeInterval = 1;
    uint64_t msTimeInterval = 0;
    std::string dumpFilePath;
    int64_t dumpTotalTime = -1;
    int dumpTimes = -1;
    // for dump raw data to file
    bool rawData;
    bool startDumpTask;
    // bool stopDumpTask;
    bool listDumpTask;
    int dumpTaskId = -1;
    bool showDate;
};

class ComletDump : public ComletBase {
   private:
    std::unique_ptr<ComletDumpOptions> opts;

    std::shared_ptr<nlohmann::json> statsJson;
    std::shared_ptr<nlohmann::json> engineUtilJson;
    std::shared_ptr<nlohmann::json> fabricThroughputJson;
    std::map<std::string, std::unique_ptr<nlohmann::json>> deviceJsons;
    std::string curDeviceId;
    std::string curTileId;

    std::atomic<bool> keepDumping;
    std::ofstream dumpFile;
    
    // MEI permission tracking
    bool meiPermissionChecked = false;
    bool hasMeiPermission = true;
    std::set<int> suppressedMeiMetrics; // Metrics to show as N/A due to lack of MEI permission

    std::string metricsHelpStr = "Metrics type to collect raw data, options. Separated by the comma.\n";
    std::set<std::string> sumMetricsList{ "XPUM_STATS_MEMORY_READ", 
                                            "XPUM_STATS_MEMORY_WRITE", 
                                            "XPUM_STATS_MEMORY_READ_THROUGHPUT", 
                                            "XPUM_STATS_MEMORY_WRITE_THROUGHPUT", 
                                            "XPUM_STATS_MEMORY_USED", 
                                            "XPUM_STATS_PCIE_READ_THROUGHPUT", 
                                            "XPUM_STATS_PCIE_WRITE_THROUGHPUT", 
                                            "XPUM_STATS_RAS_ERROR_CAT_RESET", 
                                            "XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS", 
                                            "XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS", 
                                            "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE", 
                                            "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE", 
                                            "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE", 
                                            "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE", 
                                            "XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE", 
                                            "XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE",
                                            "XPUM_STATS_ENERGY",
                                            "XPUM_STATS_POWER"
                                            };

   public:
    ComletDump() : ComletBase("dump", "Dump device statistics data.") {
        printHelpWhenNoArgs = true;
        for (std::size_t i = 0; i < dumpTypeOptions.size(); i++) {
            metricsHelpStr += std::to_string(i) + ". " + dumpTypeOptions[i].name;
            if (dumpTypeOptions[i].description.size() > 0) {
                metricsHelpStr += ", " + dumpTypeOptions[i].description;
            }
            metricsHelpStr += "\n";
        }
        keepDumping.store(true);
    }
    virtual ~ComletDump() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

    virtual void getJsonResult(std::ostream &out, bool raw = false) override;

    virtual void getTableResult(std::ostream &out) override;

    void waitForEsc();

    void printByLine(std::ostream &out);

    void printByLineWithoutInitializeCore(std::ostream &out);

    std::unique_ptr<nlohmann::json> getMetricsFromSysfs();

    void dumpRawDataToFile(std::ostream &out);

    bool dumpIdlePowerOnly();

    std::string getEnv();

    inline std::vector<std::string>  getDeviceIds(){
        return opts->deviceIds;
    }

    std::unique_ptr<nlohmann::json> combineTileAndDeviceLevel(nlohmann::json rawJson);
};
} // end namespace xpum::cli
