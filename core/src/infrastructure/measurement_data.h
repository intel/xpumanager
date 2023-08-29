/* 
 *  Copyright (C) 2021-2023 Intel Corporation
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
    SubdeviceData() {
        avg = min = max = current = std::numeric_limits<uint64_t>::max();
    }
};

struct SubdeviceRawData {
    uint64_t raw_data;
    uint64_t raw_timestamp;
    SubdeviceRawData() {
        raw_data = std::numeric_limits<uint64_t>::max();
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

struct AdditionalData {
    uint64_t current;
    bool is_raw_data;
    uint64_t raw_data;
    uint64_t raw_timestamp;
    int scale;

    AdditionalData() {
        is_raw_data = false;
        current = raw_data = 0;
        raw_timestamp = 0;
        scale = 1;
    }
};

class MeasurementData {
   public:
    ~MeasurementData() {
    }

    MeasurementData() : start_time(0),
                        latest_time(0),
                        avg(std::numeric_limits<uint64_t>::max()),
                        min(std::numeric_limits<uint64_t>::max()),
                        max(std::numeric_limits<uint64_t>::max()),
                        current(std::numeric_limits<uint64_t>::max()),
                        raw_data(std::numeric_limits<uint64_t>::max()),
                        scale(1),
                        bHasDataOnDevice(false),
                        bHasRawDataOnDevice(false),
                        raw_timestamp(0),
                        timestamp(0),
                        num_subdevice(0) {
        p_subdevice_datas = std::make_shared<std::map<uint32_t, SubdeviceData>>();
        p_subdevice_rawdatas = std::make_shared<std::map<uint32_t, SubdeviceRawData>>();
        p_extended_datas = std::make_shared<std::map<uint64_t, ExtendedMeasurementData>>();
    }

    MeasurementData(uint64_t value) : start_time(0),
                                      latest_time(0),
                                      avg(value),
                                      min(value),
                                      max(value),
                                      current(value),
                                      raw_data(std::numeric_limits<uint64_t>::max()),
                                      scale(1),
                                      bHasDataOnDevice(true),
                                      bHasRawDataOnDevice(false),
                                      raw_timestamp(0),
                                      timestamp(0),
                                      num_subdevice(0) {
        p_subdevice_datas = std::make_shared<std::map<uint32_t, SubdeviceData>>();
        p_subdevice_rawdatas = std::make_shared<std::map<uint32_t, SubdeviceRawData>>();
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
        bHasRawDataOnDevice = other.bHasRawDataOnDevice;
        p_subdevice_datas = other.p_subdevice_datas;
        p_subdevice_rawdatas = other.p_subdevice_rawdatas;
        p_extended_datas = other.p_extended_datas;
        raw_timestamp = other.raw_timestamp;
        timestamp = other.timestamp;
        subdevice_additional_data_types = other.subdevice_additional_data_types;
        subdevice_additional_datas = other.subdevice_additional_datas;
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

    void clearSubdeviceDataCurrent(uint32_t subdevice_id);

    void setSubdeviceDataMin(uint32_t subdevice_id, uint64_t data);

    void setSubdeviceDataMax(uint32_t subdevice_id, uint64_t data);

    void setSubdeviceDataAvg(uint32_t subdevice_id, uint64_t data);

    void setSubdeviceDataRawTimestamp(uint32_t subdevice_id, uint64_t data);

    uint64_t getSubdeviceDataRawTimestamp(uint32_t subdevice_id);

    const std::shared_ptr<std::map<uint32_t, SubdeviceData>> getSubdeviceDatas();

    const std::shared_ptr<std::map<uint32_t, SubdeviceRawData>> getSubdeviceRawDatas();

    uint32_t getSubdeviceDataSize();

    bool hasSubdeviceData(uint32_t subdevice_id);

    bool hasSubdeviceData() { return p_subdevice_datas->size() > 0; }

    bool hasSubdeviceRawData() { return p_subdevice_rawdatas->size() > 0; }

    uint32_t subdeviceNum() { return p_subdevice_datas->size(); }

    bool hasDataOnDevice() { return bHasDataOnDevice; }

    bool hasRawDataOnDevice() { return bHasRawDataOnDevice; }

    uint64_t getRawTimestamp() { return raw_timestamp; }

    void setRawTimestamp(uint64_t raw_time) { this->raw_timestamp = raw_time; }

    uint64_t getRawdata() { return raw_data; }

    void setRawData(uint64_t val) {
        bHasRawDataOnDevice = true;
        this->raw_data = val;
    }

    void setSubdeviceRawData(uint32_t subdevice_id, uint64_t data);

    void clearSubdeviceRawdata(uint32_t subdevice_id);

    uint64_t getSubdeviceRawData(uint32_t subdevice_id);

    void setDeviceId(const std::string device_id) { this->device_id = device_id; }

    std::string getDeviceId() { return this->device_id; }

    uint64_t getTimestamp() { return timestamp; }

    void setTimestamp(uint64_t time) { this->timestamp = time; }

    void setSubdeviceAdditionalData(uint32_t subdevice_id, MeasurementType type, uint64_t data, int scale = 1, bool is_raw_data = false, uint64_t timestamp = 0);

    std::map<uint32_t, std::map<MeasurementType, AdditionalData>> getSubdeviceAdditionalDatas();

    void insertSubdeviceAdditionalDataType(MeasurementType type);

    std::set<MeasurementType> getSubdeviceAdditionalDataTypes();

    uint32_t getSubdeviceAdditionalDataTypeSize();

    void clearSubdeviceAdditionalDataTypes();

    void clearSubdeviceAdditionalData();

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

    bool bHasRawDataOnDevice;

    std::shared_ptr<std::map<uint32_t, SubdeviceData>> p_subdevice_datas;

    std::shared_ptr<std::map<uint32_t, SubdeviceRawData>> p_subdevice_rawdatas;

    uint64_t raw_timestamp;

    uint64_t timestamp;

    uint32_t num_subdevice;

    std::shared_ptr<std::map<uint64_t, ExtendedMeasurementData>> p_extended_datas;

    std::set<MeasurementType> subdevice_additional_data_types;

    std::map<uint32_t, std::map<MeasurementType, AdditionalData>> subdevice_additional_datas;

    std::string errors;
};

} // end namespace xpum
