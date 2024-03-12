/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file perf_measurement_data.h
 */

#pragma once

#include "measurement_data.h"

namespace xpum {

struct PerfMetricData_t {
    std::string name;
    std::string type;
    uint32_t index;
    double current;
    double average;
    double total;
};

struct PerfMetricGroupData_t {
    std::string name;
    std::vector<PerfMetricData_t> data;
};

struct PerfMetricDeviceData_t {
    std::vector<PerfMetricGroupData_t> data;
};

class PerfMeasurementData : public MeasurementData {
   public:
    PerfMeasurementData();
    ~PerfMeasurementData();

    void addData(std::shared_ptr<PerfMetricDeviceData_t>& p_data);
    std::shared_ptr<std::vector<std::shared_ptr<PerfMetricDeviceData_t>>> getPerfMetricDatas();

   private:
    std::shared_ptr<std::vector<std::shared_ptr<PerfMetricDeviceData_t>>> p_device_datas;
};

} //namespace xpum