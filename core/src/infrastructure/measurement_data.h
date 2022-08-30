/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file measurement_data.h
 */

#pragma once

#include <chrono>
#include <ctime>
#include <map>
#include <set>
#include <string>

#include "const.h"
#include "logger.h"
#include "utility.h"

namespace xpum {

struct SubdeviceData {
    uint64_t avg;
    uint64_t min;
    uint64_t max;
    uint64_t current;
    uint64_t raw_data;
    uint64_t raw_timestamp;
    SubdeviceData() {
        avg = min = max = current = raw_data = std::numeric_limits<uint64_t>::max();
        raw_timestamp = 0;
    }
};

struct ExtendedMeasurementData {
    bool on_subdevice;
    uint32_t subdevice_id;
    uint32_t type;
    uint64_t active_time;
    uint64_t timestamp;
};

class MeasurementData {
   public:
    ~MeasurementData() {
    }

    MeasurementData() : avg(std::numeric_limits<uint64_t>::max()),
                        min(std::numeric_limits<uint64_t>::max()),
                        max(std::numeric_limits<uint64_t>::max()),
                        current(std::numeric_limits<uint64_t>::max()),
                        raw_data(std::numeric_limits<uint64_t>::max()),
                        scale(1),
                        bHasDataOnDevice(false),
                        raw_timestamp(0),
                        timestamp(0) {
        p_subdevice_datas = std::make_shared<std::map<uint32_t, SubdeviceData>>();
        p_extended_datas = std::make_shared<std::map<uint64_t, ExtendedMeasurementData>>();
    }

    MeasurementData(uint64_t value) : avg(value),
                                      min(value),
                                      max(value),
                                      current(value),
                                      raw_data(std::numeric_limits<uint64_t>::max()),
                                      scale(1),
                                      bHasDataOnDevice(true),
                                      raw_timestamp(0),
                                      timestamp(0) {
        p_subdevice_datas = std::make_shared<std::map<uint32_t, SubdeviceData>>();
        p_extended_datas = std::make_shared<std::map<uint64_t, ExtendedMeasurementData>>();
    }

    MeasurementData(const MeasurementData& other) {
        device_id = other.device_id;
        avg = other.avg;
        min = other.min;
        max = other.max;
        current = other.current;
        raw_data = other.raw_data;
        scale = other.scale;
        start_time = other.start_time;
        latest_time = other.latest_time;
        bHasDataOnDevice = other.bHasDataOnDevice;
        p_subdevice_datas = other.p_subdevice_datas;
        p_extended_datas = other.p_extended_datas;
        raw_timestamp = other.raw_timestamp;
        timestamp = other.timestamp;
        subdevice_additional_current_data_types = other.subdevice_additional_current_data_types;
        subdevice_additional_current_datas = other.subdevice_additional_current_datas;
        errors = other.errors;
    }

   public:
    void setAvg(uint64_t avg) { this->avg = avg; }

    void setMax(uint64_t max) { this->max = max; }

    void setMin(uint64_t min) { this->min = min; }

    void setCurrent(uint64_t current) {
        bHasDataOnDevice = true;
        this->current = current;
    }

    void setScale(uint64_t scale) { this->scale = scale; }

    void setStartTime(long long time) { this->start_time = time; }

    void setLatestTime(long long time) { this->latest_time = time; }

    uint64_t getAvg() { return this->avg; }

    uint64_t getMax() { return this->max; }

    uint64_t getMin() { return this->min; }

    uint64_t getCurrent() { return this->current; }

    uint64_t getScale() { return this->scale; }

    long long getStartTime() { return start_time; }

    long long getLatestTime() { return latest_time; }

    uint64_t getSubdeviceDataCurrent(uint32_t subdevice_id);

    uint64_t getSubdeviceDataMin(uint32_t subdevice_id);

    uint64_t getSubdeviceDataMax(uint32_t subdevice_id);

    uint64_t getSubdeviceDataAvg(uint32_t subdevice_id);

    void setSubdeviceDataCurrent(uint32_t subdevice_id, uint64_t data);

    void setSubdeviceDataMin(uint32_t subdevice_id, uint64_t data);

    void setSubdeviceDataMax(uint32_t subdevice_id, uint64_t data);

    void setSubdeviceDataAvg(uint32_t subdevice_id, uint64_t data);

    void setSubdeviceDataRawTimestamp(uint32_t subdevice_id, uint64_t data);

    uint64_t getSubdeviceDataRawTimestamp(uint32_t subdevice_id);

    const std::shared_ptr<std::map<uint32_t, SubdeviceData>> getSubdeviceDatas();

    uint32_t getSubdeviceDataSize();

    bool hasSubdeviceData(uint32_t subdevice_id);

    bool hasSubdeviceData() { return p_subdevice_datas->size() > 0; }

    uint32_t subdeviceNum() { return p_subdevice_datas->size(); }

    bool hasDataOnDevice() { return bHasDataOnDevice; }

    uint64_t getRawTimestamp() { return raw_timestamp; }

    void setRawTimestamp(uint64_t raw_time) { this->raw_timestamp = raw_time; }

    uint64_t getRawdata() { return raw_data; }

    void setRawData(uint64_t val) {
        bHasDataOnDevice = true;
        this->raw_data = val;
    }

    void setSubdeviceRawData(uint32_t subdevice_id, uint64_t data);

    uint64_t getSubdeviceRawData(uint32_t subdevice_id);

    void setDeviceId(const std::string device_id) { this->device_id = device_id; }

    std::string getDeviceId() { return this->device_id; }

    uint64_t getTimestamp() { return timestamp; }

    void setTimestamp(uint64_t time) { this->timestamp = time; }

    void setSubdeviceAdditionalCurrentData(uint32_t subdevice_id, MeasurementType type, uint64_t data);

    std::map<uint32_t, std::map<MeasurementType, uint64_t>> getSubdeviceAdditionalCurrentDatas();

    void insertSubdeviceAdditionalCurrentDataType(MeasurementType type);

    std::set<MeasurementType> getSubdeviceAdditionalCurrentDataTypes();

    uint32_t getSubdeviceAdditionalCurrentDataTypeSize();

    void clearSubdeviceAdditionalCurrentDataTypes();

    void clearSubdeviceAdditionalCurrentData();

    const std::shared_ptr<std::map<uint64_t, ExtendedMeasurementData>> getExtendedDatas();

    void addExtendedData(uint64_t key, ExtendedMeasurementData data);

    uint32_t getNumSubdevices() { return this->num_subdevice; }

    void setNumSubdevices(uint32_t num) { this->num_subdevice = num; }

    void setErrors(const std::string& errors) {
        this->errors = errors;
    }

    const std::string& getErrors() {
        return this->errors;
    }

   protected:
    std::string device_id;

    Timestamp_t start_time;

    Timestamp_t latest_time;

    uint64_t avg;

    uint64_t min;

    uint64_t max;

    uint64_t current;

    uint64_t raw_data;

    int scale;

    bool bHasDataOnDevice;

    std::shared_ptr<std::map<uint32_t, SubdeviceData>> p_subdevice_datas;

    uint64_t raw_timestamp;

    uint64_t timestamp;

    uint32_t num_subdevice;

    std::shared_ptr<std::map<uint64_t, ExtendedMeasurementData>> p_extended_datas;

    std::set<MeasurementType> subdevice_additional_current_data_types;

    std::map<uint32_t, std::map<MeasurementType, uint64_t>> subdevice_additional_current_datas;

    std::string errors;
};

} // end namespace xpum
