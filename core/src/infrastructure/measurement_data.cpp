/* 
 *  Copyright (C) 2021-2022 Intel Corporation
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

void MeasurementData::setSubdeviceAdditionalCurrentData(uint32_t subdevice_id, MeasurementType type, uint64_t data) {
    subdevice_additional_current_datas[subdevice_id][type] = data;
}

std::map<uint32_t, std::map<MeasurementType, uint64_t>> MeasurementData::getSubdeviceAdditionalCurrentDatas() {
    return subdevice_additional_current_datas;
}

void MeasurementData::insertSubdeviceAdditionalCurrentDataType(MeasurementType type) {
    subdevice_additional_current_data_types.insert(type);
}

std::set<MeasurementType> MeasurementData::getSubdeviceAdditionalCurrentDataTypes() {
    return subdevice_additional_current_data_types;
}

uint32_t MeasurementData::getSubdeviceAdditionalCurrentDataTypeSize() {
    return subdevice_additional_current_data_types.size();
}

void MeasurementData::clearSubdeviceAdditionalCurrentDataTypes() {
    subdevice_additional_current_data_types.clear();
}

void MeasurementData::clearSubdeviceAdditionalCurrentData() {
    subdevice_additional_current_datas.clear();
}

const std::shared_ptr<std::map<uint64_t, ExtendedMeasurementData>> MeasurementData::getExtendedDatas() {
    return p_extended_datas;
}

void MeasurementData::addExtendedData(uint64_t key, ExtendedMeasurementData data) {
    (*p_extended_datas)[key] = data;
}

} // end namespace xpum
