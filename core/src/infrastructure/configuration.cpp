/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file configuration.cpp
 */

#include "configuration.h"

#include <cstdlib>
#include <sstream>

#include "infrastructure/logger.h"
#include "infrastructure/utility.h"

namespace xpum {

int Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE = 500;
int Configuration::POWER_MONITOR_INTERNAL_PERIOD = 80;
int Configuration::MEMORY_BANDWIDTH_MONITOR_INTERNAL_PERIOD = 80;
int Configuration::DEVICE_THREAD_POOL_SIZE = 32;
int Configuration::DATA_HANDLER_CACHE_TIME_LIMIT = 60000;
int Configuration::CORE_TEMPERATURE_HEALTH_DEFAULT_LIMIT = 150;
int Configuration::MEMORY_TEMPERATURE_HEALTH_DEFAULT_LIMIT = 150;
int Configuration::POWER_HEALTH_DEFAULT_LIMIT = 1000;
uint32_t Configuration::CACHE_SIZE_LIMIT = 5000;
uint32_t Configuration::RAW_DATA_COLLECTION_TASK_NUM_MAX = 16;
int Configuration::EU_ACTIVE_STALL_IDLE_MONITOR_INTERNAL_PERIOD = 50;
int Configuration::EU_ACTIVE_STALL_IDLE_STREAMER_SAMPLING_PERIOD = 20000000;
bool Configuration::INITIALIZE_PCIE_MANAGER = false;
uint32_t Configuration::DEFAULT_MEASUREMENT_DATA_SCALE = 100;
uint32_t Configuration::MAX_STATISTICS_SESSION_NUM = 2;
uint32_t Configuration::MEMORY_IO_THROUGHPUT_DATA_SCALE = 1000;

std::set<MeasurementType> Configuration::enabled_metrics;

void Configuration::initEnabledMetrics() {
    char* xpum_metrics_env;
    xpum_metrics_env = std::getenv("XPUM_METRICS");
    if (xpum_metrics_env != NULL) {
        std::string env_str(xpum_metrics_env);
        XPUM_LOG_INFO("The environment variable XPUM_METRICS is detected: {}", env_str);
        std::stringstream env_ss(env_str);
        while (env_ss.good()) {
            std::string substr;
            getline(env_ss, substr, ',');
            auto pos_s = substr.find('-');
            if (pos_s != 0 && pos_s != std::string::npos && pos_s + 1 < substr.length()) {
                // support range in form of "a-b"
                int start_type_id = std::stoi(substr.substr(0, pos_s));
                int end_type_id = std::stoi(substr.substr(pos_s + 1));
                while (start_type_id <= end_type_id) {
                    xpum_stats_type_t type = (xpum_stats_type_t)start_type_id;
                    if ((int)type >= 0 && (int)type < MeasurementType::METRIC_MAX) {
                        enabled_metrics.emplace(Utility::measurementTypeFromXpumStatsType(type));
                        if ((int)type == MeasurementType::METRIC_PCIE_READ_THROUGHPUT || (int)type == MeasurementType::METRIC_PCIE_WRITE_THROUGHPUT || (int)type == MeasurementType::METRIC_PCIE_READ || (int)type == MeasurementType::METRIC_PCIE_WRITE) {
                            INITIALIZE_PCIE_MANAGER = true;
                        }
                    } else {
                        break;
                    }
                    start_type_id++;
                }
            } else {
                xpum_stats_type_t type = (xpum_stats_type_t)std::stoi(substr);
                if ((int)type >= 0 && (int)type < MeasurementType::METRIC_MAX) {
                    enabled_metrics.emplace(Utility::measurementTypeFromXpumStatsType(type));
                    if ((int)type == MeasurementType::METRIC_PCIE_READ_THROUGHPUT || (int)type == MeasurementType::METRIC_PCIE_WRITE_THROUGHPUT || (int)type == MeasurementType::METRIC_PCIE_READ || (int)type == MeasurementType::METRIC_PCIE_WRITE) {
                        INITIALIZE_PCIE_MANAGER = true;
                    }
                }
            }
        }
    } else {
        for (int metric = 0; metric < (int)MeasurementType::METRIC_MAX; metric++) {
            if (metric != (int)MeasurementType::METRIC_EU_ACTIVE && metric != (int)MeasurementType::METRIC_EU_IDLE && metric != (int)MeasurementType::METRIC_EU_STALL && metric != (int)MeasurementType::METRIC_PCIE_READ_THROUGHPUT && metric != (int)MeasurementType::METRIC_PCIE_WRITE_THROUGHPUT && metric != (int)MeasurementType::METRIC_PCIE_READ && metric != (int)MeasurementType::METRIC_PCIE_WRITE) {
                enabled_metrics.emplace((MeasurementType)metric);
            }
        }
    }
}

} // end namespace xpum
