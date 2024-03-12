/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file perf_measurement_data.cpp
 */

#include "perf_measurement_data.h"

namespace xpum {

PerfMeasurementData::PerfMeasurementData() {
    p_device_datas = std::make_shared<std::vector<std::shared_ptr<PerfMetricDeviceData_t>>>();
}

PerfMeasurementData::~PerfMeasurementData() {

}

void PerfMeasurementData::addData(std::shared_ptr<PerfMetricDeviceData_t>& data) {
    p_device_datas->push_back(data);
}

std::shared_ptr<std::vector<std::shared_ptr<PerfMetricDeviceData_t>>> PerfMeasurementData::getPerfMetricDatas() {
    return p_device_datas;
}

} //namespace xpum