/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file statistics_data_handler.cpp
 */

#include "statistics_data_handler.h"

#include "infrastructure/configuration.h"

namespace xpum {

StatisticsDataHandler::StatisticsDataHandler(MeasurementType type,
                                             std::shared_ptr<Persistency>& p_persistency)
    : DataHandler(type, p_persistency) {
}

StatisticsDataHandler::~StatisticsDataHandler() {
    close();
}

void StatisticsDataHandler::getCacheMinMaxAvg(std::string& device_id, int& min, int& max, int& avg) {
    double accumated = 0.0;
    int temp_min = std::numeric_limits<int>::max();
    int temp_max = std::numeric_limits<int>::min();
    int count = 0;
    std::deque<std::shared_ptr<SharedData>>::const_iterator iter = cache.begin();
    while (iter != cache.end()) {
        if ((*iter)->getData().find(device_id) != (*iter)->getData().end()) {
            int data = (*iter)->getData()[device_id]->getCurrent();
            if (data > temp_max) {
                temp_max = data;
            }
            if (data < temp_min) {
                temp_min = data;
            }
            accumated += data;
            count++;
        }
        iter++;
    }
    if (temp_max != std::numeric_limits<int>::min()) {
        max = temp_max;
    }
    if (temp_min != std::numeric_limits<int>::max()) {
        min = temp_min;
    }
    if (count != 0) {
        avg = accumated / count;
    }
}

void StatisticsDataHandler::handleData(std::shared_ptr<SharedData>& p_data) noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    cache.push_back(p_data);
    std::shared_ptr<SharedData>& q_data = cache.front();
    while (q_data != nullptr && p_data->getTime() - q_data->getTime() > Configuration::DATA_HANDLER_CACHE_TIME_LIMIT) {
        cache.pop_front();
        q_data = cache.front();
    }
}

std::shared_ptr<MeasurementData> StatisticsDataHandler::getLatestData(std::string& device_id) noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    if (p_latestData == nullptr) {
        return nullptr;
    }

    auto datas = p_latestData->getData();
    int min = 0;
    int max = 0;
    int avg = 0;
    getCacheMinMaxAvg(device_id, min, max, avg);
    datas[device_id]->setMin(min);
    datas[device_id]->setMax(max);
    datas[device_id]->setAvg(avg);
    return datas[device_id];
}

void StatisticsDataHandler::getLatestData(std::map<std::string, std::shared_ptr<MeasurementData>>& datas) noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    if (p_latestData == nullptr) {
        return;
    }

    auto existing_datas = p_latestData->getData();
    for (auto it = existing_datas.begin(); it != existing_datas.end(); it++) {
        datas[it->first] = it->second;
        std::string device_id = it->first;
        int min = 0;
        int max = 0;
        int avg = 0;
        getCacheMinMaxAvg(device_id, min, max, avg);
        datas[device_id]->setMin(min);
        datas[device_id]->setMax(max);
        datas[device_id]->setAvg(avg);
    }
}
} // end namespace xpum
