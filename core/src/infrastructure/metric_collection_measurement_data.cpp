/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file metric_collection_measurement_data.cpp
 */

#include "metric_collection_measurement_data.h"

namespace xpum {

void MetricCollectionMeasurementData::addMetricCollectionMeasurementData(uint64_t handle, bool on_subdevice, uint32_t subdevice_id) {
    (*p_collection_datas)[handle].on_subdevice = on_subdevice;
    (*p_collection_datas)[handle].subdevice_id = subdevice_id;
}

void MetricCollectionMeasurementData::setDataCur(uint64_t handle, uint64_t cur) {
    (*p_collection_datas)[handle].current = cur;
}

void MetricCollectionMeasurementData::setDataMin(uint64_t handle, uint64_t min) {
    if (p_collection_datas->find(handle) != p_collection_datas->end()) {
        (*p_collection_datas)[handle].min = min;
    }
}

void MetricCollectionMeasurementData::setDataMax(uint64_t handle, uint64_t max) {
    if (p_collection_datas->find(handle) != p_collection_datas->end()) {
        (*p_collection_datas)[handle].max = max;
    }
}

void MetricCollectionMeasurementData::setDataAvg(uint64_t handle, uint64_t avg) {
    if (p_collection_datas->find(handle) != p_collection_datas->end()) {
        (*p_collection_datas)[handle].avg = avg;
    }
}

uint64_t MetricCollectionMeasurementData::getDataCur(uint64_t handle) {
    if (p_collection_datas->find(handle) != p_collection_datas->end()) {
        return (*p_collection_datas)[handle].current;
    }
    return std::numeric_limits<uint64_t>::max();
}

uint64_t MetricCollectionMeasurementData::getDataMin(uint64_t handle) {
    if (p_collection_datas->find(handle) != p_collection_datas->end()) {
        return (*p_collection_datas)[handle].min;
    }
    return std::numeric_limits<uint64_t>::max();
}

uint64_t MetricCollectionMeasurementData::getDataMax(uint64_t handle) {
    if (p_collection_datas->find(handle) != p_collection_datas->end()) {
        return (*p_collection_datas)[handle].max;
    }
    return std::numeric_limits<uint64_t>::max();
}

uint64_t MetricCollectionMeasurementData::getDataAvg(uint64_t handle) {
    if (p_collection_datas->find(handle) != p_collection_datas->end()) {
        return (*p_collection_datas)[handle].avg;
    }
    return std::numeric_limits<uint64_t>::max();
}

} //namespace xpum