/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file configuration.cpp
 */

#include "configuration.h"

#include <fstream>
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <regex>
#include <unistd.h>
#include <limits.h>

#include "infrastructure/logger.h"
#include "infrastructure/utility.h"
#include "infrastructure/xpum_config.h"
#include <sys/stat.h>

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
bool Configuration::INITIALIZE_PERF_METRIC = false;

std::set<MeasurementType> Configuration::enabled_metrics;
std::vector<PerfMetric_t> Configuration::perf_metrics;
std::string Configuration::XPUM_MODE;

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
                    auto m_type = Utility::measurementTypeFromXpumStatsType(type);
                    if ((int)m_type >= 0 && (int)m_type < MeasurementType::METRIC_MAX) {
                        enabled_metrics.emplace(m_type);
                        if (!INITIALIZE_PCIE_MANAGER && 
                            (m_type == MeasurementType::METRIC_PCIE_READ_THROUGHPUT 
                            || m_type == MeasurementType::METRIC_PCIE_WRITE_THROUGHPUT 
                            || m_type == MeasurementType::METRIC_PCIE_READ 
                            || m_type == MeasurementType::METRIC_PCIE_WRITE)) {
                            INITIALIZE_PCIE_MANAGER = true;
                        }
                        if (!INITIALIZE_PERF_METRIC && 
                            (m_type == MeasurementType::METRIC_EU_ACTIVE
                            || m_type == MeasurementType::METRIC_EU_STALL
                            || m_type == MeasurementType::METRIC_EU_IDLE)) {
                            INITIALIZE_PERF_METRIC = true;
                        }
                    } else {
                        break;
                    }
                    start_type_id++;
                }
            } else {
                xpum_stats_type_t type = (xpum_stats_type_t)std::stoi(substr);
                auto m_type = Utility::measurementTypeFromXpumStatsType(type);
                if ((int)m_type >= 0 && (int)m_type < MeasurementType::METRIC_MAX) {
                    enabled_metrics.emplace(m_type);
                    if (!INITIALIZE_PCIE_MANAGER && 
                        (m_type == MeasurementType::METRIC_PCIE_READ_THROUGHPUT 
                        || m_type == MeasurementType::METRIC_PCIE_WRITE_THROUGHPUT 
                        || m_type == MeasurementType::METRIC_PCIE_READ 
                        || m_type == MeasurementType::METRIC_PCIE_WRITE)) {
                        INITIALIZE_PCIE_MANAGER = true;
                    }
                    if (!INITIALIZE_PERF_METRIC && 
                        (m_type == MeasurementType::METRIC_EU_ACTIVE
                        || m_type == MeasurementType::METRIC_EU_STALL
                        || m_type == MeasurementType::METRIC_EU_IDLE)) {
                        INITIALIZE_PERF_METRIC = true;
                    }
                }
            }
        }
    } else {
        for (int metric = 0; metric < (int)MeasurementType::METRIC_MAX; metric++) {
            if (metric != (int)MeasurementType::METRIC_EU_ACTIVE 
                && metric != (int)MeasurementType::METRIC_EU_IDLE 
                && metric != (int)MeasurementType::METRIC_EU_STALL 
                && metric != (int)MeasurementType::METRIC_PCIE_READ_THROUGHPUT 
                && metric != (int)MeasurementType::METRIC_PCIE_WRITE_THROUGHPUT 
                && metric != (int)MeasurementType::METRIC_PCIE_READ 
                && metric != (int)MeasurementType::METRIC_PCIE_WRITE
                && metric != (int)MeasurementType::METRIC_PERF) {
                enabled_metrics.emplace((MeasurementType)metric);
            }
        }
    }
}

void Configuration::initPerfMetrics() {
    perf_metrics.clear();

    std::string file_name = std::string(XPUM_CONFIG_DIR) + std::string("perf_metrics.conf");
    struct stat buffer;
    if (stat(file_name.c_str(), &buffer) != 0) {
        char exePath[PATH_MAX];
        ssize_t len = ::readlink("/proc/self/exe", exePath, sizeof(exePath));
        exePath[len] = '\0';
        std::string current_file = exePath;
        file_name = current_file.substr(0, current_file.find_last_of('/')) + "/../lib/" + Configuration::getXPUMMode() + "/config/" + std::string("perf_metrics.conf");
        if (stat(file_name.c_str(), &buffer) != 0)
            file_name = current_file.substr(0, current_file.find_last_of('/')) + "/../lib64/" + Configuration::getXPUMMode() + "/config/" + std::string("perf_metrics.conf");
    }
    
    std::ifstream conf_file(file_name);
    
    if (!conf_file.is_open()) { 
        XPUM_LOG_ERROR("couldn't open config file : {}", file_name);
        return ;
    }

    std::string line;
    auto regex = std::regex("\\s");

    while (getline(conf_file, line)) {
        if (line.empty()) {
            continue;
        }
        line.erase(0, line.find_first_not_of(" "));
        line.erase(line.find_last_not_of(" ") + 1);
        if (line[0] == '#' || line[0] == '\r' || line[0] == '\n' || line.empty()) {
            continue;
        }
        
        std::vector<std::string> columns(std::sregex_token_iterator(
                                     line.begin(), line.end(), regex, -1),
                                     std::sregex_token_iterator());
        if (columns.size() < 3) {
            XPUM_LOG_ERROR("Invalid configuration: {}", line);
            continue;
        }

        PerfMetric_t perfMetric;
        perfMetric.name = columns[0];
        perfMetric.group = columns[1];
        perfMetric.type = columns[2];
        perf_metrics.emplace_back(perfMetric);
    }

    conf_file.close();
}

} // end namespace xpum
