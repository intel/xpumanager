/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file measurement_data.h
 */

#pragma once

#include <map>
#include <set>
#include <string>
#include <memory>
#include <climits>
#include "infrastructure/measurement_type.h"

namespace xpum {

struct SubdeviceData {
    uint64_t current;
    SubdeviceData() {
        current = UINT64_MAX;
    }
};

struct AdditionalData {
    uint64_t current;
    AdditionalData() {
        current = UINT64_MAX;
    }
};

class MeasurementData {
   public:
    ~MeasurementData() {
    }

    MeasurementData() : current(UINT64_MAX),
                        scale(1),
                        bHasDataOnDevice(false),
                        timestamp(0) {
        p_subdevice_datas = std::make_shared<std::map<uint32_t, SubdeviceData>>();
    }

    MeasurementData(uint64_t value) : current(value),
                                      scale(1),
                                      bHasDataOnDevice(true),
                                      timestamp(0) {
        p_subdevice_datas = std::make_shared<std::map<uint32_t, SubdeviceData>>();
    }

    MeasurementData(const MeasurementData& other) {
        device_id = other.device_id;
        current = other.current;
        scale = other.scale;
        bHasDataOnDevice = other.bHasDataOnDevice;
        p_subdevice_datas = other.p_subdevice_datas;
        timestamp = other.timestamp;
        errors = other.errors;
        additional_datas = other.additional_datas;
    }

   public:

    void setCurrent(uint64_t current) {
        bHasDataOnDevice = true;
        this->current = current;
    }

    void setScale(uint64_t scale) { this->scale = scale; }

    uint64_t getCurrent() { return this->current; }

    uint64_t getScale() { return this->scale; }

    uint64_t getSubdeviceDataCurrent(uint32_t subdevice_id);

    void setSubdeviceDataCurrent(uint32_t subdevice_id, uint64_t data);

    void clearSubdeviceDataCurrent(uint32_t subdevice_id);

    const std::shared_ptr<std::map<uint32_t, SubdeviceData>> getSubdeviceDatas();

    uint32_t getSubdeviceDataSize();

    bool hasSubdeviceData(uint32_t subdevice_id);

    bool hasSubdeviceData() { return p_subdevice_datas->size() > 0; }

    uint32_t subdeviceNum() { return p_subdevice_datas->size(); }

    bool hasDataOnDevice() { return bHasDataOnDevice; }

    void setDeviceId(const std::string device_id) { this->device_id = device_id; }

    std::string getDeviceId() { return this->device_id; }

    uint64_t getTimestamp() { return timestamp; }

    void setTimestamp(uint64_t time) { this->timestamp = time; }

    uint32_t getNumSubdevices() { return this->num_subdevice; }

    void setNumSubdevices(uint32_t num) { this->num_subdevice = num; }

    void setErrors(const std::string& errors) {
        this->errors = errors;
    }

    const std::string& getErrors() {
        return this->errors;
    }

    bool hasAdditionalData();

    void setAdditionalData(MeasurementType type, uint64_t data);

    uint64_t getAdditionalData(MeasurementType type);

    std::set<MeasurementType> getAdditionalDataTypes();

   protected:
    std::string device_id;

    uint64_t current;

    int scale;

    bool bHasDataOnDevice;

    std::shared_ptr<std::map<uint32_t, SubdeviceData>> p_subdevice_datas;

    uint64_t timestamp;

    uint32_t num_subdevice;

    std::string errors;

    std::map<MeasurementType, AdditionalData> additional_datas;
};

} // end namespace xpum
