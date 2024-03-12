/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file multi_metrics_stats_data_handler.cpp
 */

#include "multi_metrics_stats_data_handler.h"

#include <algorithm>
#include <iostream>

#include "infrastructure/configuration.h"
#include "infrastructure/engine_measurement_data.h"

namespace xpum {

MultiMetricsStatsDataHandler::MultiMetricsStatsDataHandler(MeasurementType type,
                                                                             std::shared_ptr<Persistency>& p_persistency)
    : DataHandler(type, p_persistency) {
}

MultiMetricsStatsDataHandler::~MultiMetricsStatsDataHandler() {
    close();
}

void MultiMetricsStatsDataHandler::handleData(std::shared_ptr<SharedData>& p_data) noexcept {
    updateStatistics(p_data);
}

void MultiMetricsStatsDataHandler::resetStatistics(std::string& device_id, uint64_t session_id) {
    if (multi_sessions_data.find(session_id) != multi_sessions_data.end()) {
        if (multi_sessions_data[session_id].find(device_id) != multi_sessions_data[session_id].end()) {
            multi_sessions_data[session_id][device_id].clear();
        }
    }
}

void MultiMetricsStatsDataHandler::updateStatistics(std::shared_ptr<SharedData>& p_data) {
    std::unique_lock<std::mutex> lock(this->mutex);
    std::map<std::string, std::shared_ptr<MeasurementData>>::iterator iter = p_data->getData().begin();
    while (iter != p_data->getData().end()) {
        auto &deviceId = iter->first;
        for (uint64_t session = 0; session < Configuration::MAX_STATISTICS_SESSION_NUM; session++) {
            auto multi_metrics_measurement_datas = iter->second->getMultiMetricsDatas();
            std::map<std::string, std::map<uint64_t, Statistics_data_t>>::iterator iter_statistics = multi_sessions_data[session].find(deviceId);
            if (iter_statistics != multi_sessions_data[session].end()) {
                auto multi_metrics_datas_iter = multi_metrics_measurement_datas->begin();
                while (multi_metrics_datas_iter != multi_metrics_measurement_datas->end()) {
                    auto &metricHandle = multi_metrics_datas_iter->first;
                    auto &singleMeasurementData = multi_metrics_datas_iter->second;
                    auto stats_iter = multi_sessions_data[session][deviceId].find(uint64_t(metricHandle));
                    if (stats_iter != multi_sessions_data[session][deviceId].end()) {
                        stats_iter->second.count++;
                        if (singleMeasurementData.current < stats_iter->second.min) {
                            stats_iter->second.min = singleMeasurementData.current;
                        }
                        if (singleMeasurementData.current > stats_iter->second.max) {
                            stats_iter->second.max = singleMeasurementData.current;
                        }
                        stats_iter->second.avg = stats_iter->second.avg * (stats_iter->second.count - 1) * 1.0 / stats_iter->second.count + singleMeasurementData.current * 1.0 / stats_iter->second.count;
                        stats_iter->second.latest_time = p_data->getTime();
                    } else {
                        multi_sessions_data[session][deviceId][uint64_t(metricHandle)] = Statistics_data_t(singleMeasurementData.current, p_data->getTime());
                    }
                    multi_metrics_datas_iter++;
                }
            } else {
                auto multi_metrics_datas_iter = multi_metrics_measurement_datas->begin();
                while (multi_metrics_datas_iter != multi_metrics_measurement_datas->end()) {
                    //multi_metrics_datas_iter->first is handle (engine or fp)
                    //multi_metrics_datas_iter->second is SingleMeasurementData_t
                    multi_sessions_data[session][deviceId][uint64_t(multi_metrics_datas_iter->first)] = Statistics_data_t(multi_metrics_datas_iter->second.current, p_data->getTime());
                    multi_metrics_datas_iter++;
                }
            }
        }
        ++iter;
    }
}

std::shared_ptr<MeasurementData> MultiMetricsStatsDataHandler::getLatestData(std::string& device_id) noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    if (p_latestData == nullptr) {
        return nullptr;
    }

    auto datas = p_latestData->getData();
    if (datas.find(device_id) != datas.end()) {
        return datas[device_id];
    } else {
        return nullptr;
    }
}

std::shared_ptr<MeasurementData> MultiMetricsStatsDataHandler::getLatestStatistics(std::string& device_id, uint64_t session_id) noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    if (p_latestData == nullptr) {
        return nullptr;
    }

    auto datas = p_latestData->getData();
    if (datas.find(device_id) != datas.end() && datas[device_id] != nullptr) {
        auto cur_datas = datas[device_id];
        auto multi_metrics_measurement_datas = datas[device_id]->getMultiMetricsDatas();
        auto multi_metrics_datas_iter = multi_metrics_measurement_datas->begin();
        while (multi_metrics_datas_iter != multi_metrics_measurement_datas->end()) {
            auto &metricHandle = multi_metrics_datas_iter->first;
            auto &singleMeasurementData = multi_metrics_datas_iter->second;
            cur_datas->setDataCur(metricHandle, singleMeasurementData.current);
            cur_datas->setDataMin(metricHandle, singleMeasurementData.current);
            cur_datas->setDataMax(metricHandle, singleMeasurementData.current);
            cur_datas->setDataAvg(metricHandle, singleMeasurementData.current);
            cur_datas->setStartTime(cur_datas->getTimestamp());
            cur_datas->setLatestTime(cur_datas->getTimestamp());
            datas[device_id]->setStartTime(datas[device_id]->getTimestamp());
            datas[device_id]->setLatestTime(datas[device_id]->getTimestamp());
            ++multi_metrics_datas_iter;
        }
    }

    if (multi_sessions_data.find(session_id) != multi_sessions_data.end() && multi_sessions_data[session_id].find(device_id) != multi_sessions_data[session_id].end() && datas[device_id] != nullptr) {
        auto multi_metrics_measurement_datas = datas[device_id]->getMultiMetricsDatas();
        auto multi_metrics_datas_iter = multi_metrics_measurement_datas->begin();
        while (multi_metrics_datas_iter != multi_metrics_measurement_datas->end()) {
            auto &metricHandle = multi_metrics_datas_iter->first;
            auto cur_datas = std::static_pointer_cast<MeasurementData>(datas[device_id]);
            cur_datas->setDataMin(metricHandle, multi_sessions_data[session_id][device_id][uint64_t(metricHandle)].min);
            cur_datas->setDataMax(metricHandle, multi_sessions_data[session_id][device_id][uint64_t(metricHandle)].max);
            cur_datas->setDataAvg(metricHandle, multi_sessions_data[session_id][device_id][uint64_t(metricHandle)].avg);
            cur_datas->setStartTime(multi_sessions_data[session_id][device_id][uint64_t(metricHandle)].start_time);
            cur_datas->setLatestTime(multi_sessions_data[session_id][device_id][uint64_t(metricHandle)].latest_time);
            ++multi_metrics_datas_iter;
        }
        resetStatistics(device_id, session_id);
    }

    if (datas.find(device_id) != datas.end()) {
        return datas[device_id];
    } else {
        return nullptr;
    }
}

} // end namespace xpum
