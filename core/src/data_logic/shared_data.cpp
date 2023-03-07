/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file shared_data.cpp
 */

#include "shared_data.h"

namespace xpum {

SharedData::SharedData(
    Timestamp_t time,
    std::shared_ptr<std::map<std::string, std::shared_ptr<MeasurementData>>> datas) {
    for (auto it = datas->begin(); it != datas->end(); it++) {
        this->datas[it->first] = (it->second);
    }

    this->time = time;
}

SharedData::~SharedData() {
    datas.clear();
}

std::map<std::string, std::shared_ptr<MeasurementData>>& SharedData::getData() noexcept {
    return this->datas;
}

Timestamp_t SharedData::getTime() noexcept {
    return this->time;
}

} // end namespace xpum
