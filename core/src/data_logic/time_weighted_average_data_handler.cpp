/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file time_weighted_average_data_handler.cpp
 */

#include "time_weighted_average_data_handler.h"

#include <algorithm>
#include <iostream>

namespace xpum {

TimeWeightedAverageDataHandler::TimeWeightedAverageDataHandler(MeasurementType type,
                                                               std::shared_ptr<Persistency>& p_persistency)
    : MetricStatisticsDataHandler(type, p_persistency) {
}

TimeWeightedAverageDataHandler::~TimeWeightedAverageDataHandler() {
    close();
}

void TimeWeightedAverageDataHandler::counterOverflowDetection(std::shared_ptr<SharedData>& p_data) noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    if (p_preData == nullptr || p_data == nullptr) {
        return;
    }

    std::map<std::string, std::shared_ptr<MeasurementData>>::iterator iter = p_data->getData().begin();
    while (iter != p_data->getData().end()) {
        if (iter->second->hasDataOnDevice() 
        && p_preData->getData().find(iter->first) != p_preData->getData().end()
        && p_preData->getData()[iter->first]->hasRawDataOnDevice()
        && p_data->getData().find(iter->first) != p_data->getData().end()) {
            uint64_t pre_data = p_preData->getData()[iter->first]->getRawdata();
            uint64_t cur_data = p_data->getData()[iter->first]->getRawdata();
            if (pre_data != std::numeric_limits<uint64_t>::max() && cur_data != std::numeric_limits<uint64_t>::max()) {
                if (pre_data > cur_data) {
                    p_preData = nullptr;
                    return;
                }
            }
        }

        if (iter->second->hasSubdeviceRawData()
        && p_preData->getData().find(iter->first) != p_preData->getData().end()
        && p_preData->getData()[iter->first]->hasSubdeviceRawData()
        && p_data->getData().find(iter->first) != p_data->getData().end()) {
            auto iter_subdevice = iter->second->getSubdeviceRawDatas()->begin();
            while (iter_subdevice != iter->second->getSubdeviceRawDatas()->end() 
            && p_preData->getData()[iter->first]->getSubdeviceRawDatas()->find(iter_subdevice->first) != p_preData->getData()[iter->first]->getSubdeviceRawDatas()->end()) {
                uint64_t pre_data = p_preData->getData()[iter->first]->getSubdeviceRawData(iter_subdevice->first);
                uint64_t cur_data = p_data->getData()[iter->first]->getSubdeviceRawData(iter_subdevice->first);
                if (pre_data != std::numeric_limits<uint64_t>::max() && cur_data != std::numeric_limits<uint64_t>::max()) {
                    if (pre_data > cur_data) {
                        p_preData->getData()[iter->first]->clearSubdeviceRawdata(iter_subdevice->first);
                    }
                }
                ++iter_subdevice;
            }
        }

        ++iter;
    }
}

void TimeWeightedAverageDataHandler::calculateData(std::shared_ptr<SharedData>& p_data) {
    std::unique_lock<std::mutex> lock(this->mutex);

    std::map<std::string, std::shared_ptr<MeasurementData>>::iterator iter = p_data->getData().begin();
    while (iter != p_data->getData().end()) {
        if (iter->second->hasRawDataOnDevice() 
                && p_preData->getData().find(iter->first) != p_preData->getData().end() 
                && p_preData->getData()[iter->first]->hasRawDataOnDevice()
                && p_data->getData().find(iter->first) != p_data->getData().end()) {
            uint64_t pre_data = p_preData->getData()[iter->first]->getRawdata();
            uint64_t pre_data_raw_timestamp = p_preData->getData()[iter->first]->getRawTimestamp();
            uint64_t cur_data = p_data->getData()[iter->first]->getRawdata();
            uint64_t cur_data_raw_timestamp = p_data->getData()[iter->first]->getRawTimestamp();
            if (pre_data != std::numeric_limits<uint64_t>::max() && cur_data != std::numeric_limits<uint64_t>::max()) {
                if (cur_data_raw_timestamp - pre_data_raw_timestamp != 0) {
                    p_data->getData()[iter->first]->setCurrent((cur_data - pre_data) / (cur_data_raw_timestamp - pre_data_raw_timestamp));
                }
            }
        }

        if (iter->second->hasSubdeviceRawData() 
                && p_preData->getData().find(iter->first) != p_preData->getData().end() 
                && p_preData->getData()[iter->first]->hasSubdeviceRawData() 
                && p_data->getData().find(iter->first) != p_data->getData().end()) {
            std::map<uint32_t, SubdeviceRawData>::const_iterator iter_subdevice = iter->second->getSubdeviceRawDatas()->begin();
            while (iter_subdevice != iter->second->getSubdeviceRawDatas()->end() 
                    && p_preData->getData()[iter->first]->getSubdeviceRawDatas()->find(iter_subdevice->first) != p_preData->getData()[iter->first]->getSubdeviceRawDatas()->end()) {
                uint64_t pre_data = p_preData->getData()[iter->first]->getSubdeviceRawData(iter_subdevice->first);
                uint64_t pre_data_raw_timestamp = p_preData->getData()[iter->first]->getSubdeviceDataRawTimestamp(iter_subdevice->first);
                uint64_t cur_data = iter_subdevice->second.raw_data;
                uint64_t cur_data_raw_timestamp = iter_subdevice->second.raw_timestamp;
                if (pre_data != std::numeric_limits<uint64_t>::max() && cur_data != std::numeric_limits<uint64_t>::max()) {
                    if (cur_data_raw_timestamp - pre_data_raw_timestamp != 0) {
                        p_data->getData()[iter->first]->setSubdeviceDataCurrent(iter_subdevice->first, (cur_data - pre_data) / (cur_data_raw_timestamp - pre_data_raw_timestamp));
                    }
                }
                ++iter_subdevice;
            }
        }

        ++iter;
    }
}

void TimeWeightedAverageDataHandler::handleData(std::shared_ptr<SharedData>& p_data) noexcept {
    if (p_preData == nullptr || p_data == nullptr) {
        return;
    }

    counterOverflowDetection(p_data);
    calculateData(p_data);
    updateStatistics(p_data);
}
} // end namespace xpum
