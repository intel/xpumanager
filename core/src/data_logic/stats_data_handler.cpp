/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file metric_statistics_data_handler.cpp
 */

#include "stats_data_handler.h"

#include <iostream>

#include "infrastructure/configuration.h"
#include "infrastructure/utility.h"

namespace xpum {

StatsDataHandler::StatsDataHandler(MeasurementType type,
                                                         std::shared_ptr<Persistency>& p_persistency)
    : DataHandler(type, p_persistency) {
}

StatsDataHandler::~StatsDataHandler() {
    close();
}

void StatsDataHandler::resetStatistics(std::string& device_id, uint64_t session_id) {
    if (multi_sessions_data.find(session_id) != multi_sessions_data.end()) {
        multi_sessions_data[session_id].erase(device_id);
    }
}

void StatsDataHandler::updateStatistics(std::shared_ptr<SharedData>& p_data) {
    std::unique_lock<std::mutex> lock(this->mutex);
    std::map<std::string, std::shared_ptr<MeasurementData>>::iterator iter = p_data->getData().begin();
    while (iter != p_data->getData().end()) {
        auto &deviceId = iter->first;
        auto &measurementData = iter->second;
        for (uint64_t session = 0; session < Configuration::MAX_STATISTICS_SESSION_NUM; session++) {
            std::map<std::string, Statistics_data>::iterator iter_statistics = multi_sessions_data[session].find(deviceId);
            if (iter_statistics != multi_sessions_data[session].end()) {
                auto &stats = iter_statistics->second;
                stats.count++;
                if (measurementData->hasDataOnDevice()) {
                    stats.hasDataOnDevice = true;
                    if (measurementData->getCurrent() < stats.min) {
                        stats.min = measurementData->getCurrent();
                    }
                    if (measurementData->getCurrent() > stats.max) {
                        stats.max = measurementData->getCurrent();
                    }
                    stats.avg = stats.avg * (stats.count - 1) * 1.0 / stats.count + measurementData->getCurrent() * 1.0 / stats.count;
                } else {
                    stats.hasDataOnDevice = false;
                }
                stats.latest_time = p_data->getTime();
            } else {
                if (measurementData->getCurrent() != std::numeric_limits<uint64_t>::max()) {
                    multi_sessions_data[session].insert(std::make_pair(deviceId, Statistics_data(measurementData->getCurrent(), p_data->getTime())));
                }
            }

            std::map<uint32_t, SubdeviceData>::const_iterator iter_subdevice = measurementData->getSubdeviceDatas()->begin();
            while (iter_subdevice != measurementData->getSubdeviceDatas()->end()) {
                auto &subDeviceId = iter_subdevice->first;
                if (multi_sessions_data[session].find(deviceId) == multi_sessions_data[session].end()) {
                    multi_sessions_data[session].insert(std::make_pair(deviceId, Statistics_data(subDeviceId, measurementData->getSubdeviceDataCurrent(subDeviceId), p_data->getTime())));
                    continue;
                }
                std::map<uint32_t, Statistics_subdevice_data>::iterator iter_subdevice_statistics = multi_sessions_data[session].find(deviceId)->second.subdevice_datas.find(subDeviceId);
                if (iter_subdevice_statistics != multi_sessions_data[session].find(deviceId)->second.subdevice_datas.end()) {
                    auto &subStats = iter_subdevice_statistics->second;
                    if (measurementData->getSubdeviceDataCurrent(subDeviceId) != std::numeric_limits<uint64_t>::max()) {
                        subStats.count++;
                        uint64_t current_data = measurementData->getSubdeviceDataCurrent(subDeviceId);
                        if (current_data < subStats.min) {
                            subStats.min = current_data;
                        }
                        if (current_data > subStats.max) {
                            subStats.max = current_data;
                        }
                        subStats.avg = (subStats.avg * (subStats.count - 1) + measurementData->getSubdeviceDataCurrent(iter_subdevice_statistics->first)) * 1.0 / subStats.count;
                    }
                } else {
                    if (measurementData->getSubdeviceDataCurrent(subDeviceId) != std::numeric_limits<uint64_t>::max()) {
                        if (multi_sessions_data[session].find(deviceId) == multi_sessions_data[session].end()) {
                            multi_sessions_data[session].insert(std::make_pair(deviceId, Statistics_data(subDeviceId, measurementData->getSubdeviceDataCurrent(subDeviceId), p_data->getTime())));
                        } else {
                            multi_sessions_data[session].find(deviceId)->second.subdevice_datas.insert(std::make_pair(subDeviceId, Statistics_subdevice_data(measurementData->getSubdeviceDataCurrent(subDeviceId))));
                        }
                    }
                }
                ++iter_subdevice;
            }
        }
        ++iter;
    }
}

void StatsDataHandler::handleData(std::shared_ptr<SharedData>& p_data) noexcept {
    updateStatistics(p_data);
}

std::shared_ptr<MeasurementData> StatsDataHandler::getLatestData(std::string& device_id) noexcept {
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

std::shared_ptr<MeasurementData> StatsDataHandler::getLatestStatistics(std::string& device_id, uint64_t session_id) noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    if (p_latestData == nullptr) {
        return nullptr;
    }

    auto datas = p_latestData->getData();
    if (datas.find(device_id) != datas.end() && datas[device_id] != nullptr) {
        datas[device_id]->setAvg(datas[device_id]->getCurrent());
        datas[device_id]->setMin(datas[device_id]->getCurrent());
        datas[device_id]->setMax(datas[device_id]->getCurrent());
        datas[device_id]->setStartTime(datas[device_id]->getTimestamp());
        datas[device_id]->setLatestTime(datas[device_id]->getTimestamp());
        auto sub = datas[device_id]->getSubdeviceDatas()->begin();
        //The first sub is subDeviceId 
        //The second of sub is Statistics_subdevice_data
        while (sub != datas[device_id]->getSubdeviceDatas()->end()) {
            sub->second.avg = sub->second.current;
            sub->second.min = sub->second.current;
            sub->second.max = sub->second.current;
            sub++;
        }
    }

    if (multi_sessions_data.find(session_id) != multi_sessions_data.end() && datas.find(device_id) != datas.end() && datas[device_id] != nullptr) {
        std::map<std::string, Statistics_data>::iterator iter = multi_sessions_data[session_id].find(device_id);
        auto &stats = iter->second;
        if (iter != multi_sessions_data[session_id].end()) {
            datas[device_id]->setAvg(stats.avg);
            datas[device_id]->setMin(stats.min);
            datas[device_id]->setMax(stats.max);
            datas[device_id]->setStartTime(stats.start_time);
            datas[device_id]->setLatestTime(stats.latest_time);
            //The first sub is subDeviceId 
            //The second of sub is Statistics_subdevice_data
            for (auto& sub : stats.subdevice_datas) {
                datas[device_id]->setSubdeviceDataAvg(sub.first, sub.second.avg);
                datas[device_id]->setSubdeviceDataMin(sub.first, sub.second.min);
                datas[device_id]->setSubdeviceDataMax(sub.first, sub.second.max);
            }
            resetStatistics(device_id, session_id);
        }
    }

    if (datas.find(device_id) != datas.end()) {
        return datas[device_id];
    } else {
        return nullptr;
    }
}
} // end namespace xpum
