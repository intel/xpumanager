/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file statistics_data_handler.cpp
 */

#include "avg_data_handler.h"

#include "infrastructure/configuration.h"

namespace xpum {

AvgDataHandler::AvgDataHandler(MeasurementType type,
                                             std::shared_ptr<Persistency>& p_persistency)
    : DataHandler(type, p_persistency) {
}

AvgDataHandler::~AvgDataHandler() {
    close();
}

void AvgDataHandler::getAvg(std::string& device_id, int& min, int& max, int& avg) {
    double accumated = 0.0;
    int temp_min = std::numeric_limits<int>::max();
    int temp_max = std::numeric_limits<int>::min();
    int count = 0;
    std::deque<std::shared_ptr<SharedData>>::const_iterator iter = deque.begin();
    while (iter != deque.end()) {
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

void AvgDataHandler::handleData(std::shared_ptr<SharedData>& p_data) noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    deque.push_back(p_data);
    std::shared_ptr<SharedData>& q_data = deque.front();
    while (q_data != nullptr && p_data->getTime() - q_data->getTime() > Configuration::DATA_HANDLER_CACHE_TIME_LIMIT) {
        deque.pop_front();
        q_data = deque.front();
    }
}

std::shared_ptr<MeasurementData> AvgDataHandler::getLatestData(std::string& device_id) noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    if (p_latestData == nullptr) {
        return nullptr;
    }

    auto datas = p_latestData->getData();
    int min = 0;
    int max = 0;
    int avg = 0;
    getAvg(device_id, min, max, avg);
    datas[device_id]->setMin(min);
    datas[device_id]->setMax(max);
    datas[device_id]->setAvg(avg);
    return datas[device_id];
}

} // end namespace xpum
