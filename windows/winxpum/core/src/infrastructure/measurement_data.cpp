/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file measurement_data.cpp
 */

#include "pch.h"
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

bool MeasurementData::hasSubdeviceData(uint32_t subdevice_id) {
    return p_subdevice_datas->find(subdevice_id) != p_subdevice_datas->end();
}

bool MeasurementData::hasAdditionalData() {
    return additional_datas.size() > 0;
}

void MeasurementData::setAdditionalData(MeasurementType type, uint64_t data) {
    AdditionalData d;
    d.current = data;
    additional_datas[type] = d;
}

uint64_t MeasurementData::getAdditionalData(MeasurementType type) {
    return additional_datas[type].current;
}

std::set<MeasurementType> MeasurementData::getAdditionalDataTypes() {
    std::set<MeasurementType> types;
    for (auto data : additional_datas)
        types.insert(data.first);
    return types;
}

uint64_t MeasurementData::getSubdeviceDataCurrent(uint32_t subdevice_id) {
    if (p_subdevice_datas->find(subdevice_id) != p_subdevice_datas->end()) {
        return (*p_subdevice_datas)[subdevice_id].current;
    }
    return UINT64_MAX;
}

const std::shared_ptr<std::map<uint32_t, SubdeviceData>> MeasurementData::getSubdeviceDatas() {
    return p_subdevice_datas;
}

uint32_t MeasurementData::getSubdeviceDataSize() {
    return p_subdevice_datas->size();
}

} // end namespace xpum
