/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file gpu_utilization_data_handler.cpp
 */


#include <algorithm>
#include <iostream>

#include "core/core.h"
#include "infrastructure/configuration.h"
#include "perf_measurement_data.h"
#include "perf_metrics_data_handler.h"


namespace xpum {

PerfMetricsHandler::PerfMetricsHandler(MeasurementType type,
                                       std::shared_ptr<Persistency>& p_persistency)
    : MetricStatisticsDataHandler(type, p_persistency) {
}

PerfMetricsHandler::~PerfMetricsHandler() {
    close();
}

void PerfMetricsHandler::calculateData(std::shared_ptr<SharedData>& p_data) {
    std::unique_lock<std::mutex> lock(this->mutex);

    auto all_device_data = p_data->getData();
    for (auto it = all_device_data.begin(); it != all_device_data.end(); it++) {
        std::shared_ptr<PerfMeasurementData> p_measurement_data = std::static_pointer_cast<PerfMeasurementData>(it->second);
        std::cout << "Device Id:" << it->first << std::endl;
        auto p_perf_datas = p_measurement_data->getDatas();
        for (size_t i = 0; i < p_perf_datas->size(); i++) {
            std::cout << "Sub device:" << i << std::endl;
            for (auto group_data : (*p_perf_datas)[i]->data) {
                std::cout << "Metric group:" << group_data.name << std::endl;
                for (auto metric_data : group_data.data) {
                    std::cout << "Metric name:" << metric_data.name << " Value:" << metric_data.average << std::endl;
                }
            }
        }        
    }
}

void PerfMetricsHandler::handleData(std::shared_ptr<SharedData>& p_data) noexcept {
    if (p_preData == nullptr || p_data == nullptr) {
        return;
    }

    calculateData(p_data);
    updateStatistics(p_data);
}
} // end namespace xpum
