/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file data_handler.cpp
 */

#include "data_handler.h"

#include <thread>

#include "infrastructure/logger.h"

namespace xpum {

DataHandler::DataHandler(MeasurementType type,
                         std::shared_ptr<Persistency>& p_persistency)
    : type(type),
      p_persistency(p_persistency) {
    stop = false;
    p_latestData = nullptr;
    p_preData = nullptr;
}

DataHandler::~DataHandler() {
    close();
}

void DataHandler::init() {
}

void DataHandler::close() {
    stop = true;
}

void DataHandler::preHandleData(std::shared_ptr<SharedData>& p_data) noexcept {
    // handle data in the caller thread directly, don't put any time consuming task here

    std::unique_lock<std::mutex> lock(this->mutex);
    this->p_preData = this->p_latestData;
    this->p_latestData = p_data;
    auto iter = this->p_latestData->getData().begin();
    while (iter != this->p_latestData->getData().end()) {
        iter->second->setTimestamp(p_data->getTime());
        ++iter;
    }
    lock.unlock();

    if (p_data != nullptr) {
        try {
            this->p_persistency->storeMeasurementData(this->type, p_data->getTime(), p_data->getData());
        } catch (std::exception& e) {
            std::string error = "Failed to persist measurement data:";
            error += e.what();
            XPUM_LOG_ERROR(error);
        } catch (...) {
            std::string error = "Failed to persist measurement data: unexpected exception";
            XPUM_LOG_ERROR(error);
        }
    }
}

std::shared_ptr<MeasurementData> DataHandler::getLatestData(std::string& device_id) noexcept {
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

std::shared_ptr<MeasurementData> DataHandler::getLatestStatistics(std::string& device_id, uint64_t session_id) noexcept {
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

void DataHandler::getLatestData(std::map<std::string, std::shared_ptr<MeasurementData>>& datas) noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    if (p_latestData == nullptr) {
        return;
    }

    auto existing_datas = p_latestData->getData();
    for (auto it = existing_datas.begin(); it != existing_datas.end(); it++) {
        datas[it->first] = it->second;
    }
}

} // end namespace xpum
