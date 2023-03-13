/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file metric_statistics_data_handler.cpp
 */

#include "metric_statistics_data_handler.h"

#include <iostream>

#include "infrastructure/configuration.h"
#include "infrastructure/utility.h"

namespace xpum {

MetricStatisticsDataHandler::MetricStatisticsDataHandler(MeasurementType type,
                                                         std::shared_ptr<Persistency>& p_persistency)
    : DataHandler(type, p_persistency) {
}

MetricStatisticsDataHandler::~MetricStatisticsDataHandler() {
    close();
}

void MetricStatisticsDataHandler::resetStatistics(std::string& device_id, uint64_t session_id) {
    if (statistics_datas.find(session_id) != statistics_datas.end()) {
        statistics_datas[session_id].erase(device_id);
    }
}

void MetricStatisticsDataHandler::updateStatistics(std::shared_ptr<SharedData>& p_data) {
    std::unique_lock<std::mutex> lock(this->mutex);
    std::map<std::string, std::shared_ptr<MeasurementData>>::iterator iter = p_data->getData().begin();
    while (iter != p_data->getData().end()) {
        for (uint64_t session = 0; session < Configuration::MAX_STATISTICS_SESSION_NUM; session++) {
            std::map<std::string, Statistics_data>::iterator iter_statistics = statistics_datas[session].find(iter->first);
            if (iter_statistics != statistics_datas[session].end()) {
                iter_statistics->second.count++;
                if (iter->second->hasDataOnDevice()) {
                    iter_statistics->second.hasDataOnDevice = true;
                    if (iter->second->getCurrent() < iter_statistics->second.min) {
                        iter_statistics->second.min = iter->second->getCurrent();
                    }
                    if (iter->second->getCurrent() > iter_statistics->second.max) {
                        iter_statistics->second.max = iter->second->getCurrent();
                    }
                    iter_statistics->second.avg = iter_statistics->second.avg * (iter_statistics->second.count - 1) * 1.0 / iter_statistics->second.count + iter->second->getCurrent() * 1.0 / iter_statistics->second.count;
                } else {
                    iter_statistics->second.hasDataOnDevice = false;
                }
                iter_statistics->second.latest_time = p_data->getTime();
            } else {
                if (iter->second->getCurrent() != std::numeric_limits<uint64_t>::max()) {
                    statistics_datas[session].insert(std::make_pair(iter->first, Statistics_data(iter->second->getCurrent(), p_data->getTime())));
                }
            }

            std::map<uint32_t, SubdeviceData>::const_iterator iter_subdevice = iter->second->getSubdeviceDatas()->begin();
            while (iter_subdevice != iter->second->getSubdeviceDatas()->end()) {
                if (statistics_datas[session].find(iter->first) == statistics_datas[session].end()) {
                    statistics_datas[session].insert(std::make_pair(iter->first, Statistics_data(iter_subdevice->first, iter->second->getSubdeviceDataCurrent(iter_subdevice->first), p_data->getTime())));
                    continue;
                }
                std::map<uint32_t, Statistics_subdevice_data>::iterator iter_subdevice_statistics = statistics_datas[session].find(iter->first)->second.subdevice_datas.find(iter_subdevice->first);
                if (iter_subdevice_statistics != statistics_datas[session].find(iter->first)->second.subdevice_datas.end()) {
                    if (iter->second->getSubdeviceDataCurrent(iter_subdevice_statistics->first) != std::numeric_limits<uint64_t>::max()) {
                        iter_subdevice_statistics->second.count++;
                        uint64_t current_data = iter->second->getSubdeviceDataCurrent(iter_subdevice_statistics->first);
                        if (current_data < iter_subdevice_statistics->second.min) {
                            iter_subdevice_statistics->second.min = current_data;
                        }
                        if (current_data > iter_subdevice_statistics->second.max) {
                            iter_subdevice_statistics->second.max = current_data;
                        }
                        iter_subdevice_statistics->second.avg = (iter_subdevice_statistics->second.avg * (iter_subdevice_statistics->second.count - 1) + iter->second->getSubdeviceDataCurrent(iter_subdevice_statistics->first)) * 1.0 / iter_subdevice_statistics->second.count;
                    }
                } else {
                    if (iter->second->getSubdeviceDataCurrent(iter_subdevice_statistics->first) != std::numeric_limits<uint64_t>::max()) {
                        if (statistics_datas[session].find(iter->first) == statistics_datas[session].end()) {
                            statistics_datas[session].insert(std::make_pair(iter->first, Statistics_data(iter_subdevice->first, iter->second->getSubdeviceDataCurrent(iter_subdevice_statistics->first), p_data->getTime())));
                        } else {
                            statistics_datas[session].find(iter->first)->second.subdevice_datas.insert(std::make_pair(iter_subdevice->first, Statistics_subdevice_data(iter->second->getSubdeviceDataCurrent(iter_subdevice_statistics->first))));
                        }
                    }
                }
                ++iter_subdevice;
            }
        }
        ++iter;
    }
}

void MetricStatisticsDataHandler::handleData(std::shared_ptr<SharedData>& p_data) noexcept {
    updateStatistics(p_data);
}

std::shared_ptr<MeasurementData> MetricStatisticsDataHandler::getLatestData(std::string& device_id) noexcept {
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

std::shared_ptr<MeasurementData> MetricStatisticsDataHandler::getLatestStatistics(std::string& device_id, uint64_t session_id) noexcept {
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
        while (sub != datas[device_id]->getSubdeviceDatas()->end()) {
            sub->second.avg = sub->second.current;
            sub->second.min = sub->second.current;
            sub->second.max = sub->second.current;
            sub++;
        }
    }

    if (statistics_datas.find(session_id) != statistics_datas.end() && datas[device_id] != nullptr) {
        std::map<std::string, Statistics_data>::iterator iter = statistics_datas[session_id].find(device_id);
        if (iter != statistics_datas[session_id].end()) {
            datas[device_id]->setAvg(iter->second.avg);
            datas[device_id]->setMin(iter->second.min);
            datas[device_id]->setMax(iter->second.max);
            datas[device_id]->setStartTime(iter->second.start_time);
            datas[device_id]->setLatestTime(iter->second.latest_time);
            for (auto& sub : iter->second.subdevice_datas) {
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
