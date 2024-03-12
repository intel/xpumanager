/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file engine_group_utilization_data_handler.cpp
 */

#include "engine_group_utilization_data_handler.h"

#include <algorithm>
#include <iostream>

#include "core/core.h"
#include "infrastructure/configuration.h"

namespace xpum {

EngineGroupUtilizationDataHandler::EngineGroupUtilizationDataHandler(MeasurementType type,
                                                                     std::shared_ptr<Persistency>& p_persistency)
    : StatsDataHandler(type, p_persistency) {
}

EngineGroupUtilizationDataHandler::~EngineGroupUtilizationDataHandler() {
    close();
}

uint32_t EngineGroupUtilizationDataHandler::getAverage(std::vector<uint32_t>& datas) {
    uint32_t sum = 0;
    for (auto& data : datas) {
        sum += data;
    }
    if (datas.size() != 0) {
        return sum / datas.size();
    } else {
        return 0;
    }
}

void EngineGroupUtilizationDataHandler::calculateData(std::shared_ptr<SharedData>& p_data) {
    std::unique_lock<std::mutex> lock(this->mutex);

    std::map<std::string, std::shared_ptr<MeasurementData>>::iterator iter = p_data->getData().begin();
    while (iter != p_data->getData().end()) {
        auto &deviceId = iter->first;
        auto &measurementData = iter->second;
        Property prop;
        Core::instance().getDeviceManager()->getDevice(deviceId)->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_NAME, prop);

        auto extended_data = measurementData->getExtendedDatas()->begin();
        while (extended_data != measurementData->getExtendedDatas()->end()) {
            auto &engineHandle = extended_data->first;
            auto &exMeasurementData =extended_data->second;
            auto pre_data_iter = p_preData->getData().find(deviceId);
            if (pre_data_iter != p_preData->getData().end()) {
                auto pre_extended = pre_data_iter->second->getExtendedDatas()->find(engineHandle);
                if (pre_extended != pre_data_iter->second->getExtendedDatas()->end()) {
                    auto &preExMeasurementData = pre_extended->second;
                    if (exMeasurementData.type == ZES_ENGINE_GROUP_COMPUTE_ALL || exMeasurementData.type == ZES_ENGINE_GROUP_RENDER_ALL || 
                        exMeasurementData.type == ZES_ENGINE_GROUP_MEDIA_ALL || exMeasurementData.type == ZES_ENGINE_GROUP_COPY_ALL || 
                        exMeasurementData.type == ZES_ENGINE_GROUP_3D_ALL) {
                        if (exMeasurementData.timestamp == 
                                preExMeasurementData.timestamp) {
                            ++extended_data;
                            continue;
                        }
                        uint64_t val = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * 100 * 
                            (exMeasurementData.active_time - preExMeasurementData.active_time) / 
                            (exMeasurementData.timestamp - preExMeasurementData.timestamp);
                        if (val > Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * 100) {
                            val = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * 100;
                        }
                        p_data->getData()[deviceId]->setScale(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                        if (exMeasurementData.on_subdevice) {
                            p_data->getData()[deviceId]->setSubdeviceDataCurrent(exMeasurementData.subdevice_id, val);
                        } else {
                            p_data->getData()[deviceId]->setCurrent(val);
                        }
                    }
                }
            }
            ++extended_data;
        }

        ++iter;
    }
}

void EngineGroupUtilizationDataHandler::handleData(std::shared_ptr<SharedData>& p_data) noexcept {
    if (p_preData == nullptr || p_data == nullptr) {
        return;
    }

    calculateData(p_data);
    updateStatistics(p_data);
}

} // end namespace xpum
