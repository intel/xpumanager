/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file configuration.cpp
 */

#pragma warning(disable: 4996)

#include "pch.h"
#include "configuration.h"

#include "infrastructure/logger.h"
#include "infrastructure/utility.h"

#include <cstdlib>

namespace xpum {

    int Configuration::POWER_MONITOR_INTERNAL_PERIOD = 10;
    int Configuration::MEMORY_BANDWIDTH_MONITOR_INTERNAL_PERIOD = 80;
    int Configuration::MEMORY_READ_WRITE_INTERNAL_PERIOD = 100;
    int Configuration::ENGINE_GPU_UTILIZATION_INTERNAL_PERIOD = 110;
    uint32_t Configuration::DEFAULT_MEASUREMENT_DATA_SCALE = 100;
    int Configuration::EU_ACTIVE_STALL_IDLE_MONITOR_INTERNAL_PERIOD = 100;
    int Configuration::EU_ACTIVE_STALL_IDLE_STREAMER_SAMPLING_PERIOD = 20000000;
    std::set<MeasurementType> Configuration::enabled_metrics;

    void Configuration::initEnabledMetrics() {
        char* xpum_metrics_env = nullptr;
        size_t sz = 0;
        if (_dupenv_s(&xpum_metrics_env, &sz, "XPUM_METRICS") == 0 && xpum_metrics_env != nullptr) {
            std::string env_str(xpum_metrics_env);
            free(xpum_metrics_env);
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
                        }
                        else {
                            break;
                        }
                        start_type_id++;
                    }
                }
                else {
                    xpum_stats_type_t type = (xpum_stats_type_t)std::stoi(substr);
                    auto m_type = Utility::measurementTypeFromXpumStatsType(type);
                    if ((int)m_type >= 0 && (int)m_type < MeasurementType::METRIC_MAX) {
                        enabled_metrics.emplace(m_type);
                    }
                }
            }
        }
        else {
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
} // end namespace xpum
