/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file metric_collection_measurement_data.h
 */

#pragma once

#include "measurement_data.h"

namespace xpum {

struct MetricCollectionMeasurementData_t {
    bool on_subdevice;
    uint32_t subdevice_id;
    uint64_t current;
    uint64_t max;
    uint64_t min;
    uint64_t avg;
    MetricCollectionMeasurementData_t() {
        on_subdevice = false;
        subdevice_id = std::numeric_limits<uint32_t>::max();
        avg = min = max = current = std::numeric_limits<uint64_t>::max();
    };
};

class MetricCollectionMeasurementData : public MeasurementData {
   public:
    MetricCollectionMeasurementData() {
        p_collection_datas = std::make_shared<std::map<uint64_t, MetricCollectionMeasurementData_t>>();
    }
    ~MetricCollectionMeasurementData() {
        p_collection_datas->clear();
    }
    const std::shared_ptr<std::map<uint64_t, MetricCollectionMeasurementData_t>> getDatas() {
        return p_collection_datas;
    }
    void addMetricCollectionMeasurementData(uint64_t handle, bool on_subdevice, uint32_t subdevice_id);
    void setDataCur(uint64_t handle, uint64_t cur);
    void setDataMin(uint64_t handle, uint64_t min);
    void setDataMax(uint64_t handle, uint64_t max);
    void setDataAvg(uint64_t handle, uint64_t avg);
    uint64_t getDataCur(uint64_t handle);
    uint64_t getDataMin(uint64_t handle);
    uint64_t getDataMax(uint64_t handle);
    uint64_t getDataAvg(uint64_t handle);

   private:
    std::shared_ptr<std::map<uint64_t, MetricCollectionMeasurementData_t>> p_collection_datas;
};

} //namespace xpum