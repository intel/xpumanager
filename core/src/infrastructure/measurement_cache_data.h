/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file measurement_cache_data.h
 */

#pragma once

#include <chrono>
#include <ctime>
#include <map>
#include <string>

#include "const.h"
#include "logger.h"
#include "measurement_type.h"
#include "utility.h"

namespace xpum {

class MeasurementCacheData {
   public:
    MeasurementCacheData(const std::string& deviceId, MeasurementType metricType, uint64_t value, long long timeStamp, bool onSubdevice, uint32_t subdeviceID)
        : device_id(deviceId), type(metricType), data(value), time(timeStamp), on_subdevice(onSubdevice), subdevice_id(subdeviceID) {
    }

    MeasurementCacheData(const MeasurementCacheData& other) {
        time = other.time;
        data = other.data;
        type = other.type;
        on_subdevice = other.on_subdevice;
        subdevice_id = other.subdevice_id;
        device_id = other.device_id;
    }

    virtual ~MeasurementCacheData() {
    }

   public:
    Timestamp_t getTime() { return this->time; }

    uint64_t getData() { return this->data; }

    MeasurementType getType() { return this->type; }

    bool onSubdevice() { return this->on_subdevice; }

    uint32_t getSubdeviceID() { return this->subdevice_id; }

    std::string getDeviceId() { return this->device_id; }

   protected:
    std::string device_id;

    MeasurementType type;

    uint64_t data;

    Timestamp_t time;

    bool on_subdevice;

    int32_t subdevice_id;
};

} // end namespace xpum
