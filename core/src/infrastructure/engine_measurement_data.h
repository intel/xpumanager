/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file engine_measurement_data.h
 */

#pragma once

#include "measurement_data.h"

namespace xpum {

struct EngineRawData_t {
    zes_engine_group_t type;
    uint64_t raw_active_time;
    uint64_t raw_timestamp;
    EngineRawData_t() {
        type = ZES_ENGINE_GROUP_FORCE_UINT32;
        raw_active_time = std::numeric_limits<uint64_t>::max();
        raw_timestamp = 0;
    };
};

class EngineCollectionMeasurementData : public MeasurementData {
   public:
    EngineCollectionMeasurementData() {
        p_engine_datas = std::make_shared<std::map<uint64_t, EngineRawData_t>>();
    }
    ~EngineCollectionMeasurementData() {
        p_engine_datas->clear();
    }
    void addRawData(uint64_t handle, zes_engine_group_t type, bool on_subdevice, uint32_t subdevice_id, uint64_t raw_active_time, uint64_t raw_timestamp);
    const std::shared_ptr<std::map<uint64_t, EngineRawData_t>> getEngineRawDatas() {
        return p_engine_datas;
    }
    zes_engine_group_t getEngineType(uint64_t handle);

   private:
    std::shared_ptr<std::map<uint64_t, EngineRawData_t>> p_engine_datas;
};

} //namespace xpum