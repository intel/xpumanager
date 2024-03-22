/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file measurement_data.cpp
 */

#include "measurement_data.h"

namespace xpum {

void MeasurementData::setSubdeviceDataCurrent(uint32_t subdevice_id, uint64_t data) {
    (*p_subdevice_datas)[subdevice_id].current = data;
}

void MeasurementData::clearSubdeviceDataCurrent(uint32_t subdevice_id) {
    auto iter = p_subdevice_datas->find(subdevice_id);
    if (iter != p_subdevice_datas->end()) {
        p_subdevice_datas->erase(iter);
    }
}

void MeasurementData::setSubdeviceDataRawTimestamp(uint32_t subdevice_id, uint64_t data) {
    (*p_subdevice_rawdatas)[subdevice_id].raw_timestamp = data;
}

void MeasurementData::setSubdeviceRawData(uint32_t subdevice_id, uint64_t data) {
    (*p_subdevice_rawdatas)[subdevice_id].raw_data = data;
}

void MeasurementData::clearSubdeviceRawdata(uint32_t subdevice_id) {
    auto iter = p_subdevice_rawdatas->find(subdevice_id);
    if (iter != p_subdevice_rawdatas->end()) {
        p_subdevice_rawdatas->erase(iter);
    }
}

void MeasurementData::setSubdeviceDataMin(uint32_t subdevice_id, uint64_t data) {
    (*p_subdevice_datas)[subdevice_id].min = data;
}

void MeasurementData::setSubdeviceDataMax(uint32_t subdevice_id, uint64_t data) {
    (*p_subdevice_datas)[subdevice_id].max = data;
}

void MeasurementData::setSubdeviceDataAvg(uint32_t subdevice_id, uint64_t data) {
    (*p_subdevice_datas)[subdevice_id].avg = data;
}

bool MeasurementData::hasSubdeviceData(uint32_t subdevice_id) {
    return p_subdevice_datas->find(subdevice_id) != p_subdevice_datas->end();
}

uint64_t MeasurementData::getSubdeviceDataCurrent(uint32_t subdevice_id) {
    if (p_subdevice_datas->find(subdevice_id) != p_subdevice_datas->end()) {
        return (*p_subdevice_datas)[subdevice_id].current;
    }
    return std::numeric_limits<uint64_t>::max();
}

uint64_t MeasurementData::getSubdeviceDataMin(uint32_t subdevice_id) {
    if (p_subdevice_datas->find(subdevice_id) != p_subdevice_datas->end()) {
        return (*p_subdevice_datas)[subdevice_id].min;
    }
    return std::numeric_limits<uint64_t>::max();
}

uint64_t MeasurementData::getSubdeviceDataMax(uint32_t subdevice_id) {
    if (p_subdevice_datas->find(subdevice_id) != p_subdevice_datas->end()) {
        return (*p_subdevice_datas)[subdevice_id].max;
    }
    return std::numeric_limits<uint64_t>::max();
}

uint64_t MeasurementData::getSubdeviceDataAvg(uint32_t subdevice_id) {
    if (p_subdevice_datas->find(subdevice_id) != p_subdevice_datas->end()) {
        return (*p_subdevice_datas)[subdevice_id].avg;
    }
    return std::numeric_limits<uint64_t>::max();
}

uint64_t MeasurementData::getSubdeviceDataRawTimestamp(uint32_t subdevice_id) {
    if (p_subdevice_rawdatas->find(subdevice_id) != p_subdevice_rawdatas->end()) {
        return (*p_subdevice_rawdatas)[subdevice_id].raw_timestamp;
    }
    return std::numeric_limits<uint64_t>::max();
}

uint64_t MeasurementData::getSubdeviceRawData(uint32_t subdevice_id) {
    if (p_subdevice_rawdatas->find(subdevice_id) != p_subdevice_rawdatas->end()) {
        return (*p_subdevice_rawdatas)[subdevice_id].raw_data;
    }
    return std::numeric_limits<uint64_t>::max();
}

const std::shared_ptr<std::map<uint32_t, SubdeviceData>> MeasurementData::getSubdeviceDatas() {
    return p_subdevice_datas;
}

const std::shared_ptr<std::map<uint32_t, SubdeviceRawData>> MeasurementData::getSubdeviceRawDatas() {
    return p_subdevice_rawdatas;
}

uint32_t MeasurementData::getSubdeviceDataSize() {
    return p_subdevice_datas->size();
}

void MeasurementData::setSubdeviceAdditionalData(uint32_t subdevice_id, MeasurementType type, uint64_t data, int scale, bool is_raw_data, uint64_t timestamp) {
    AdditionalData add_data;
    add_data.scale = scale;
    if (is_raw_data) {
        add_data.is_raw_data = is_raw_data;
        add_data.raw_data = data;
        add_data.raw_timestamp = timestamp;
    } else {
        add_data.current = data;
    }
    subdevice_additional_datas[subdevice_id][type] = add_data;
    subdevice_additional_data_types.insert(type);
}

std::map<uint32_t, std::map<MeasurementType, AdditionalData>> MeasurementData::getSubdeviceAdditionalDatas() {
    return subdevice_additional_datas;
}

void MeasurementData::insertSubdeviceAdditionalDataType(MeasurementType type) {
    subdevice_additional_data_types.insert(type);
}

std::set<MeasurementType> MeasurementData::getSubdeviceAdditionalDataTypes() {
    return subdevice_additional_data_types;
}

uint32_t MeasurementData::getSubdeviceAdditionalDataTypeSize() {
    return subdevice_additional_data_types.size();
}

void MeasurementData::clearSubdeviceAdditionalDataTypes() {
    subdevice_additional_data_types.clear();
}

void MeasurementData::clearSubdeviceAdditionalData() {
    subdevice_additional_datas.clear();
}

const std::shared_ptr<std::map<uint64_t, ExtendedMeasurementData>> MeasurementData::getExtendedDatas() {
    return p_extended_datas;
}

void MeasurementData::addExtendedData(uint64_t key, ExtendedMeasurementData data) {
    (*p_extended_datas)[key] = data;
}

void MeasurementData::addSingleMeasurementData(uint64_t handle, bool on_subdevice, uint32_t subdevice_id) {
    (*p_multi_metrics_datas)[handle].on_subdevice = on_subdevice;
    (*p_multi_metrics_datas)[handle].subdevice_id = subdevice_id;
}

void MeasurementData::setDataCur(uint64_t handle, uint64_t cur) {
    (*p_multi_metrics_datas)[handle].current = cur;
}

void MeasurementData::setDataMin(uint64_t handle, uint64_t min) {
    if (p_multi_metrics_datas->find(handle) != p_multi_metrics_datas->end()) {
        (*p_multi_metrics_datas)[handle].min = min;
    }
}

void MeasurementData::setDataMax(uint64_t handle, uint64_t max) {
    if (p_multi_metrics_datas->find(handle) != p_multi_metrics_datas->end()) {
        (*p_multi_metrics_datas)[handle].max = max;
    }
}

void MeasurementData::setDataAvg(uint64_t handle, uint64_t avg) {
    if (p_multi_metrics_datas->find(handle) != p_multi_metrics_datas->end()) {
        (*p_multi_metrics_datas)[handle].avg = avg;
    }
}

uint64_t MeasurementData::getDataCur(uint64_t handle) {
    if (p_multi_metrics_datas->find(handle) != p_multi_metrics_datas->end()) {
        return (*p_multi_metrics_datas)[handle].current;
    }
    return std::numeric_limits<uint64_t>::max();
}

uint64_t MeasurementData::getDataMin(uint64_t handle) {
    if (p_multi_metrics_datas->find(handle) != p_multi_metrics_datas->end()) {
        return (*p_multi_metrics_datas)[handle].min;
    }
    return std::numeric_limits<uint64_t>::max();
}

uint64_t MeasurementData::getDataMax(uint64_t handle) {
    if (p_multi_metrics_datas->find(handle) != p_multi_metrics_datas->end()) {
        return (*p_multi_metrics_datas)[handle].max;
    }
    return std::numeric_limits<uint64_t>::max();
}

uint64_t MeasurementData::getDataAvg(uint64_t handle) {
    if (p_multi_metrics_datas->find(handle) != p_multi_metrics_datas->end()) {
        return (*p_multi_metrics_datas)[handle].avg;
    }
    return std::numeric_limits<uint64_t>::max();
}

} // end namespace xpum
