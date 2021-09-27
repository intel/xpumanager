#pragma once

#include <string>
#include <chrono>
#include <ctime>
#include <map>
#include "const.h"
#include "logger.h"
#include "utility.h"

struct SubdeviceData {
  uint64_t avg;
  uint64_t min;
  uint64_t max;
  uint64_t current;
};

struct AddictionalData {
  uint64_t avg;
  uint64_t min;
  uint64_t max;
  uint64_t current;
};

class MeasurementData {
 public:
  ~MeasurementData() {
  }

  MeasurementData(): avg(-1), min(-1), 
    max(-1), current(-1), scale(1), bHasDataOnDevice(false) {
  }

  MeasurementData(uint64_t value): avg(value), 
    min(value), max(value), current(value), scale(1), bHasDataOnDevice(true) {
  }  

  MeasurementData(uint64_t value, uint64_t scale): avg(value), 
    min(value), max(value), current(value), scale(scale), bHasDataOnDevice(true) {
  }

  MeasurementData(uint64_t avg, uint64_t min, uint64_t max): avg(avg), 
    min(min), max(max), current(-1), scale(1), bHasDataOnDevice(true) {
  }

  MeasurementData(uint64_t avg, uint64_t min, uint64_t max, uint64_t current, uint64_t scale): 
    avg(avg), min(min), max(max), current(current), scale(scale), bHasDataOnDevice(true) {
  }

  MeasurementData(const MeasurementData& other) {
    avg = other.avg;
    min = other.min;
    max = other.max;
    current = other.current;
    scale = other.scale;
    start_time = other.start_time;
    latest_time = other.latest_time;
    bHasDataOnDevice = other.bHasDataOnDevice;
    subdevice_datas = other.subdevice_datas;
    additional_datas = other.additional_datas;
    subdevice_additional_datas = other.subdevice_additional_datas;
  }

 public:  
  void setAvg(uint64_t avg) { this->avg = avg; }

  void setMax(uint64_t max) { this->max = max; }

  void setMin(uint64_t min) { this->min = min; }

  void setCurrent(uint64_t current) { bHasDataOnDevice = true; this->current = current; }

  void setScale(uint64_t scale) { this->scale = scale; }

  void setDeviceId(const std::string& device_id) { this->device_id = scale; }

  void setStartTime(long long time) { this->start_time = time; }

  void setLatestTime(long long time) { this->latest_time = time; }

  uint64_t getAvg() { return this->avg; }

  uint64_t getMax() { return this->max; }

  uint64_t getMin() { return this->min; }

  uint64_t getCurrent() { return this->current; }

  uint64_t getScale() { return this->scale; }

  std::string getDeviceId() { return this->device_id; }

  long long getStartTime() {return start_time;}

  long long getLatestTime() {return latest_time;}

  uint64_t getSubdeviceDataCurrent(uint32_t subdevice_id);

  uint64_t getSubdeviceDataMin(uint32_t subdevice_id);

  uint64_t getSubdeviceDataMax(uint32_t subdevice_id);

  uint64_t getSubdeviceDataAvg(uint32_t subdevice_id);

  void setSubdeviceDataCurrent(uint32_t subdevice_id, uint64_t data);

  void setSubdeviceDataMin(uint32_t subdevice_id, uint64_t data);

  void setSubdeviceDataMax(uint32_t subdevice_id, uint64_t data);

  void setSubdeviceDataAvg(uint32_t subdevice_id, uint64_t data);

  const std::map<uint32_t, SubdeviceData>& getSubdeviceDatas();

  uint32_t getSubdeviceDataSize();

  bool hasSubdeviceData();

  uint32_t subdeviceNum() { return subdevice_datas.size(); }

  bool hasDataOnDevice() { return bHasDataOnDevice; }

  uint64_t getAdditionalDataCurrent(std::string name);

  uint64_t getAdditionalDataMin(std::string name);

  uint64_t getAdditionalDataMax(std::string name);

  uint64_t getAdditionalDataAvg(std::string name);

  uint64_t getSubdeviceAdditionalDataCurrent(uint32_t subdevice_id, std::string name);

  uint64_t getSubdeviceAdditionalDataMin(uint32_t subdevice_id, std::string name);

  uint64_t getSubdeviceAdditionalDataMax(uint32_t subdevice_id, std::string name);

  uint64_t getSubdeviceAdditionalDataAvg(uint32_t subdevice_id, std::string name);

  void setAdditionalDataCurrent(std::string name, uint64_t value);

  void setAdditionalDataMin(std::string name, uint64_t value);

  void setAdditionalDataMax(std::string name, uint64_t value);

  void setAdditionalDataAvg(std::string name, uint64_t value);

  void setSubdeviceAdditionalDataCurrent(uint32_t subdevice_id, std::string name, uint64_t value);

  void setSubdeviceAdditionalDataMin(uint32_t subdevice_id, std::string name, uint64_t value);

  void setSubdeviceAdditionalDataMax(uint32_t subdevice_id, std::string name, uint64_t value);

  void setSubdeviceAdditionalDataAvg(uint32_t subdevice_id, std::string name, uint64_t value);

  bool hasSubdeviceAdditionalData();

 protected:
  std::string device_id;  

  Timestamp_t start_time;

  Timestamp_t latest_time;

  uint64_t avg;

  uint64_t min;

  uint64_t max;

  uint64_t current;

  int scale;

  bool bHasDataOnDevice;

  std::map<uint32_t, SubdeviceData> subdevice_datas;

  std::map<std::string, AddictionalData> additional_datas;

  std::map<uint32_t, std::map<std::string, AddictionalData>> subdevice_additional_datas;
};
