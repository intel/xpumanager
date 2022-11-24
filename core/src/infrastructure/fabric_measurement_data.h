/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file fabric_measurement_data.h
 */

#pragma once

#include "measurement_data.h"
#include "metric_collection_measurement_data.h"

namespace xpum {

struct FabricRawData_t {
    uint64_t timestamp;
    uint64_t rx_counter;
    uint64_t tx_counter;
    uint32_t attach_id;
    uint32_t remote_fabric_id;
    uint32_t remote_attach_id;
    FabricRawData_t() {
        timestamp = 0;
        rx_counter = 0;
        tx_counter = 0;
        attach_id = std::numeric_limits<uint32_t>::max();
        remote_fabric_id = std::numeric_limits<uint32_t>::max();
        remote_attach_id = std::numeric_limits<uint32_t>::max();
    };
};

class FabricMeasurementData : public MetricCollectionMeasurementData {
   public:
    FabricMeasurementData() {
        p_fabric_datas = std::make_shared<std::map<uint64_t, FabricRawData_t>>();
    }
    ~FabricMeasurementData() {
        p_fabric_datas->clear();
    }
    void addRawData(uint64_t handle, uint64_t timestamp, uint64_t rx_counter, uint64_t tx_counter, uint32_t attach_id, uint32_t remote_fabric_id, uint32_t remote_attach_id);
    const std::shared_ptr<std::map<uint64_t, FabricRawData_t>> getFabricRawDatas() {
        return p_fabric_datas;
    }

   private:
    std::shared_ptr<std::map<uint64_t, FabricRawData_t>> p_fabric_datas;
};

} //namespace xpum