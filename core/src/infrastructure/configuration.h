/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file configuration.h
 */

#pragma once
#include <set>
#include <vector>
#include <string>

#include "measurement_type.h"

namespace xpum {

struct PerfMetric_t {
    std::string name;
    std::string group;
    std::string type;
};

class Configuration {
   public:
    static int TELEMETRY_DATA_MONITOR_FREQUENCE;
    static int POWER_MONITOR_INTERNAL_PERIOD;
    static int MEMORY_BANDWIDTH_MONITOR_INTERNAL_PERIOD;
    static int DEVICE_THREAD_POOL_SIZE;
    static int DATA_HANDLER_CACHE_TIME_LIMIT;
    static int CORE_TEMPERATURE_HEALTH_DEFAULT_LIMIT;
    static int MEMORY_TEMPERATURE_HEALTH_DEFAULT_LIMIT;
    static int POWER_HEALTH_DEFAULT_LIMIT;
    static u_int32_t RAW_DATA_COLLECTION_TASK_NUM_MAX;
    static u_int32_t CACHE_SIZE_LIMIT;
    static int EU_ACTIVE_STALL_IDLE_MONITOR_INTERNAL_PERIOD;
    static int EU_ACTIVE_STALL_IDLE_STREAMER_SAMPLING_PERIOD;
    static bool INITIALIZE_PCIE_MANAGER;
    static uint32_t DEFAULT_MEASUREMENT_DATA_SCALE;
    static uint32_t MAX_STATISTICS_SESSION_NUM;
    static bool INITIALIZE_PERF_METRIC;

   public:
    static void init() {
        initEnabledMetrics();
        initPerfMetrics();
    }

    static void initEnabledMetrics();
    static void initPerfMetrics();

    static std::set<MeasurementType>& getEnabledMetrics() {
        return enabled_metrics;
    }
    
    static std::vector<PerfMetric_t>& getPerfMetrics() {
        return perf_metrics;
    }

   private:
    static std::set<MeasurementType> enabled_metrics;
    static std::vector<PerfMetric_t> perf_metrics;
};

} // end namespace xpum
