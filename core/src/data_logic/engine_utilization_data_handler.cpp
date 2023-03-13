/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file engine_utilization_data_handler.cpp
 */

#include "engine_utilization_data_handler.h"

#include <algorithm>
#include <iostream>

#include "infrastructure/configuration.h"
#include "infrastructure/engine_measurement_data.h"

namespace xpum {

EngineUtilizationDataHandler::EngineUtilizationDataHandler(MeasurementType type,
                                                           std::shared_ptr<Persistency>& p_persistency)
    : MetricCollectionStatisticsDataHandler(type, p_persistency) {
}

EngineUtilizationDataHandler::~EngineUtilizationDataHandler() {
    close();
}

void EngineUtilizationDataHandler::calculateData(std::shared_ptr<SharedData>& p_data) {
    std::unique_lock<std::mutex> lock(this->mutex);
    std::map<std::string, std::shared_ptr<MeasurementData>>::iterator iter = p_data->getData().begin();
    while (iter != p_data->getData().end()) {
        auto pre_iter = p_preData->getData().find(iter->first);
        if (pre_iter != p_preData->getData().end()) {
            auto cur_engine_datas = std::static_pointer_cast<EngineCollectionMeasurementData>(iter->second)->getEngineRawDatas();
            auto pre_engine_datas = std::static_pointer_cast<EngineCollectionMeasurementData>(pre_iter->second)->getEngineRawDatas();
            auto cur_engine_datas_iter = cur_engine_datas->begin();
            while (cur_engine_datas_iter != cur_engine_datas->end()) {
                auto pre_engine_datas_iter = pre_engine_datas->find(cur_engine_datas_iter->first);
                if (pre_engine_datas_iter != pre_engine_datas->end()) {
                    auto cur_active_time = cur_engine_datas_iter->second.raw_active_time;
                    auto cur_timestamp = cur_engine_datas_iter->second.raw_timestamp;
                    auto pre_active_time = pre_engine_datas_iter->second.raw_active_time;
                    auto pre_timestamp = pre_engine_datas_iter->second.raw_timestamp;
                    if (cur_timestamp - pre_timestamp != 0) {
                        uint64_t val = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * 100 * (cur_active_time - pre_active_time) / (cur_timestamp - pre_timestamp);
                        if (val > Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * 100) {
                            val = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * 100;
                        }
                        std::static_pointer_cast<EngineCollectionMeasurementData>(iter->second)->setDataCur(cur_engine_datas_iter->first, val);
                        iter->second->setScale(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                    }
                }

                ++cur_engine_datas_iter;
            }
        }
        ++iter;
    }
}

void EngineUtilizationDataHandler::handleData(std::shared_ptr<SharedData>& p_data) noexcept {
    if (p_preData == nullptr || p_data == nullptr) {
        return;
    }

    calculateData(p_data);
    updateStatistics(p_data);
}

} // end namespace xpum
