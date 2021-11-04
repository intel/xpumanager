#pragma once

#include <chrono>
#include <ctime>
#include <map>
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
};

class MeasurementData {
   public:
    ~MeasurementData() {
    }

    MeasurementData() : avg(-1), min(-1), max(-1), current(-1), raw_data(-1), scale(1), bHasDataOnDevice(false), raw_timestamp(0), timestamp(0) {
        p_subdevice_datas = std::make_shared<std::map<uint32_t, SubdeviceData>>();
    }

    MeasurementData(uint64_t value) : avg(value),
                                      min(value),
                                      max(value),
                                      current(value),
                                      raw_data(-1),
                                      scale(1),
                                      bHasDataOnDevice(true),
                                      raw_timestamp(0), 
                                      timestamp(0) {
                                          p_subdevice_datas = std::make_shared<std::map<uint32_t, SubdeviceData>>();
    }

    MeasurementData(uint64_t value, uint64_t scale) : avg(value),
                                                      min(value),
                                                      max(value),
                                                      current(value),
                                                      raw_data(-1),
                                                      scale(scale),
                                                      bHasDataOnDevice(true),
                                                      raw_timestamp(0), 
                                                      timestamp(0) {
                                                          p_subdevice_datas = std::make_shared<std::map<uint32_t, SubdeviceData>>();
    }

    MeasurementData(uint64_t avg, uint64_t min, uint64_t max) : avg(avg),
                                                                min(min),
                                                                max(max),
                                                                current(-1),
                                                                raw_data(-1),
                                                                scale(1),
                                                                bHasDataOnDevice(true),
                                                                raw_timestamp(0),
                                                                timestamp(0) {
                                                                    p_subdevice_datas = std::make_shared<std::map<uint32_t, SubdeviceData>>();
    }

    MeasurementData(uint64_t avg, uint64_t min, uint64_t max, uint64_t current, uint64_t scale) : avg(avg), min(min), max(max), current(current), raw_data(-1), scale(scale), bHasDataOnDevice(true), raw_timestamp(0), timestamp(0) {
        p_subdevice_datas = std::make_shared<std::map<uint32_t, SubdeviceData>>();
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
        raw_timestamp = other.raw_timestamp;
        timestamp = other.timestamp;
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

    bool hasSubdeviceData();

    uint32_t subdeviceNum() { return p_subdevice_datas->size(); }

    bool hasDataOnDevice() { return bHasDataOnDevice; }

    uint64_t getRawTimestamp() { return raw_timestamp; }

    void setRawTimestamp(uint64_t raw_time) { this->raw_timestamp = raw_time; }

    uint64_t getRawdata() { return raw_data; }

    void setRawData(uint64_t val) { this->raw_data = val; }

    void setSubdeviceRawData(uint32_t subdevice_id, uint64_t data);

    uint64_t getSubdeviceRawData(uint32_t subdevice_id);

    void setDeviceId(const std::string device_id) { this->device_id = device_id; }

    std::string getDeviceId() { return this->device_id; }

    uint64_t getTimestamp() { return timestamp; }

    void setTimestamp(uint64_t time) { this->timestamp = time; }

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
};

} // end namespace xpum
