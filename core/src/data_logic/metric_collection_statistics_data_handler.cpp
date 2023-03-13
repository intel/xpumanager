/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file metric_collection_statistics_data_handler.cpp
 */

#include "metric_collection_statistics_data_handler.h"

#include <algorithm>
#include <iostream>

#include "infrastructure/configuration.h"
#include "infrastructure/engine_measurement_data.h"

namespace xpum {

MetricCollectionStatisticsDataHandler::MetricCollectionStatisticsDataHandler(MeasurementType type,
                                                                             std::shared_ptr<Persistency>& p_persistency)
    : DataHandler(type, p_persistency) {
}

MetricCollectionStatisticsDataHandler::~MetricCollectionStatisticsDataHandler() {
    close();
}

void MetricCollectionStatisticsDataHandler::handleData(std::shared_ptr<SharedData>& p_data) noexcept {
    updateStatistics(p_data);
}

void MetricCollectionStatisticsDataHandler::resetStatistics(std::string& device_id, uint64_t session_id) {
    if (statistics_datas.find(session_id) != statistics_datas.end()) {
        if (statistics_datas[session_id].find(device_id) != statistics_datas[session_id].end()) {
            statistics_datas[session_id][device_id].clear();
        }
    }
}

void MetricCollectionStatisticsDataHandler::updateStatistics(std::shared_ptr<SharedData>& p_data) {
    std::unique_lock<std::mutex> lock(this->mutex);
    std::map<std::string, std::shared_ptr<MeasurementData>>::iterator iter = p_data->getData().begin();
    while (iter != p_data->getData().end()) {
        for (uint64_t session = 0; session < Configuration::MAX_STATISTICS_SESSION_NUM; session++) {
            auto metric_collection_datas = std::static_pointer_cast<MetricCollectionMeasurementData>(iter->second)->getDatas();
            std::map<std::string, std::map<uint64_t, Statistics_data_t>>::iterator iter_statistics = statistics_datas[session].find(iter->first);
            if (iter_statistics != statistics_datas[session].end()) {
                auto metric_collection_datas_iter = metric_collection_datas->begin();
                while (metric_collection_datas_iter != metric_collection_datas->end()) {
                    auto stats_iter = statistics_datas[session][iter->first].find(uint64_t(metric_collection_datas_iter->first));
                    if (stats_iter != statistics_datas[session][iter->first].end()) {
                        stats_iter->second.count++;
                        if (metric_collection_datas_iter->second.current < stats_iter->second.min) {
                            stats_iter->second.min = metric_collection_datas_iter->second.current;
                        }
                        if (metric_collection_datas_iter->second.current > stats_iter->second.max) {
                            stats_iter->second.max = metric_collection_datas_iter->second.current;
                        }
                        stats_iter->second.avg = stats_iter->second.avg * (stats_iter->second.count - 1) * 1.0 / stats_iter->second.count + metric_collection_datas_iter->second.current * 1.0 / stats_iter->second.count;
                        stats_iter->second.latest_time = p_data->getTime();
                    } else {
                        statistics_datas[session][iter->first][uint64_t(metric_collection_datas_iter->first)] = Statistics_data_t(metric_collection_datas_iter->second.current, p_data->getTime());
                    }
                    metric_collection_datas_iter++;
                }
            } else {
                auto metric_collection_datas_iter = metric_collection_datas->begin();
                while (metric_collection_datas_iter != metric_collection_datas->end()) {
                    statistics_datas[session][iter->first][uint64_t(metric_collection_datas_iter->first)] = Statistics_data_t(metric_collection_datas_iter->second.current, p_data->getTime());
                    metric_collection_datas_iter++;
                }
            }
        }
        ++iter;
    }
}

std::shared_ptr<MeasurementData> MetricCollectionStatisticsDataHandler::getLatestData(std::string& device_id) noexcept {
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

std::shared_ptr<MeasurementData> MetricCollectionStatisticsDataHandler::getLatestStatistics(std::string& device_id, uint64_t session_id) noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    if (p_latestData == nullptr) {
        return nullptr;
    }

    auto datas = p_latestData->getData();
    if (datas.find(device_id) != datas.end() && datas[device_id] != nullptr) {
        auto cur_datas = std::static_pointer_cast<MetricCollectionMeasurementData>(datas[device_id]);
        auto metric_collection_datas = std::static_pointer_cast<MetricCollectionMeasurementData>(datas[device_id])->getDatas();
        auto metric_collection_datas_iter = metric_collection_datas->begin();
        while (metric_collection_datas_iter != metric_collection_datas->end()) {
            cur_datas->setDataCur(metric_collection_datas_iter->first, metric_collection_datas_iter->second.current);
            cur_datas->setDataMin(metric_collection_datas_iter->first, metric_collection_datas_iter->second.current);
            cur_datas->setDataMax(metric_collection_datas_iter->first, metric_collection_datas_iter->second.current);
            cur_datas->setDataAvg(metric_collection_datas_iter->first, metric_collection_datas_iter->second.current);
            cur_datas->setStartTime(cur_datas->getTimestamp());
            cur_datas->setLatestTime(cur_datas->getTimestamp());
            datas[device_id]->setStartTime(datas[device_id]->getTimestamp());
            datas[device_id]->setLatestTime(datas[device_id]->getTimestamp());
            ++metric_collection_datas_iter;
        }
    }

    if (statistics_datas.find(session_id) != statistics_datas.end() && statistics_datas[session_id].find(device_id) != statistics_datas[session_id].end() && datas[device_id] != nullptr) {
        auto metric_collection_datas = std::static_pointer_cast<MetricCollectionMeasurementData>(datas[device_id])->getDatas();
        auto metric_collection_datas_iter = metric_collection_datas->begin();
        while (metric_collection_datas_iter != metric_collection_datas->end()) {
            auto cur_datas = std::static_pointer_cast<MetricCollectionMeasurementData>(datas[device_id]);
            cur_datas->setDataMin(metric_collection_datas_iter->first, statistics_datas[session_id][device_id][uint64_t(metric_collection_datas_iter->first)].min);
            cur_datas->setDataMax(metric_collection_datas_iter->first, statistics_datas[session_id][device_id][uint64_t(metric_collection_datas_iter->first)].max);
            cur_datas->setDataAvg(metric_collection_datas_iter->first, statistics_datas[session_id][device_id][uint64_t(metric_collection_datas_iter->first)].avg);
            cur_datas->setStartTime(statistics_datas[session_id][device_id][uint64_t(metric_collection_datas_iter->first)].start_time);
            cur_datas->setLatestTime(statistics_datas[session_id][device_id][uint64_t(metric_collection_datas_iter->first)].latest_time);
            ++metric_collection_datas_iter;
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
