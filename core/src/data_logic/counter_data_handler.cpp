/* 
 *  Copyright (C) 2021-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file counter_data_handler.cpp
 */

#include "counter_data_handler.h"
#include <algorithm>
#include <iostream>

namespace xpum {

CounterDataHandler::CounterDataHandler(MeasurementType type, 
                                                   std::shared_ptr<Persistency>& p_persistency) 
: StatsDataHandler(type, p_persistency) {
}

CounterDataHandler::~CounterDataHandler() {
    close();
}

void CounterDataHandler::counterOverflowDetection(std::shared_ptr<SharedData>& p_data) noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    if (p_preData == nullptr || p_data == nullptr) {
        return;
    }

    std::map<std::string, std::shared_ptr<MeasurementData>>::iterator iter = p_data->getData().begin();
    while (iter != p_data->getData().end()) {
        auto &deviceId = iter->first;
        auto &measurementData = iter->second;
        if (measurementData->hasDataOnDevice() 
        && p_preData->getData().find(deviceId) != p_preData->getData().end()
        && p_data->getData().find(deviceId) != p_data->getData().end()) {
            uint64_t pre_data = p_preData->getData()[deviceId]->getCurrent();
            uint64_t cur_data = p_data->getData()[deviceId]->getCurrent();
            if (pre_data > cur_data) {
                p_preData = nullptr;
                return;
            }   
        }

        if (measurementData->hasSubdeviceData()
        && p_preData->getData().find(deviceId) != p_preData->getData().end()
        && p_preData->getData()[deviceId]->hasSubdeviceData()
        && p_data->getData().find(deviceId) != p_data->getData().end()) {
            std::map<uint32_t, SubdeviceData>::const_iterator iter_subdevice = measurementData->getSubdeviceDatas()->begin();
            auto &subDeviceId = iter_subdevice->first;
            auto &subDeviceData = iter_subdevice->second;
            while (iter_subdevice != measurementData->getSubdeviceDatas()->end() 
            && p_preData->getData()[deviceId]->getSubdeviceDatas()->find(subDeviceId) != p_preData->getData()[deviceId]->getSubdeviceDatas()->end()) {
                uint64_t pre_data = p_preData->getData()[deviceId]->getSubdeviceDataCurrent(subDeviceId);
                uint64_t cur_data = subDeviceData.current;
                if (pre_data != std::numeric_limits<uint64_t>::max() && cur_data != std::numeric_limits<uint64_t>::max()) {
                    if (pre_data > cur_data) {
                        p_preData->getData()[deviceId]->clearSubdeviceDataCurrent(subDeviceId);
                    }
                }
                ++iter_subdevice;
            }
        }

        ++iter;
    }
}

void CounterDataHandler::handleData(std::shared_ptr<SharedData>& p_data) noexcept {
    if (p_preData == nullptr || p_data == nullptr) {
        return;
    }
    
    counterOverflowDetection(p_data);
    updateStatistics(p_data);
}

} // end namespace xpum
