/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file engine_measurement_data.cpp
 */

#include "engine_measurement_data.h"

namespace xpum {

void EngineCollectionMeasurementData::addRawData(uint64_t handle,
                                                 zes_engine_group_t type,
                                                 bool on_subdevice,
                                                 uint32_t subdevice_id,
                                                 uint64_t raw_active_time,
                                                 uint64_t raw_timestamp) {
    (*p_engine_datas)[handle].type = type;
    (*p_engine_datas)[handle].raw_active_time = raw_active_time;
    (*p_engine_datas)[handle].raw_timestamp = raw_timestamp;
    addMetricCollectionMeasurementData(handle, on_subdevice, subdevice_id);
}

zes_engine_group_t EngineCollectionMeasurementData::getEngineType(uint64_t handle) {
    if (p_engine_datas->find(handle) != p_engine_datas->end()) {
        return (*p_engine_datas)[handle].type;
    }
    return ZES_ENGINE_GROUP_FORCE_UINT32;
}

} //namespace xpum