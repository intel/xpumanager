#pragma once

#include <string>
#include <chrono>
#include <ctime>

#include "const.h"
#include "logger.h"

class MeasurementData {
 public:
  ~MeasurementData() {
  }

  MeasurementData(): avg(-1), min(-1), 
    max(-1), current(-1), scale(1) {
  }

  MeasurementData(uint64_t value): avg(value), 
    min(value), max(value), current(value), scale(1) {
  }  

  MeasurementData(uint64_t value, uint64_t scale): avg(value), 
    min(value), max(value), current(value), scale(scale) {
  }

  MeasurementData(uint64_t avg, uint64_t min, uint64_t max): avg(avg), 
    min(min), max(max), current(-1), scale(1) {
  }

  MeasurementData(uint64_t avg, uint64_t min, uint64_t max, uint64_t current, uint64_t scale): 
    avg(avg), min(min), max(max), current(current), scale(scale) {
  }

  MeasurementData(const MeasurementData& other) {
    avg = other.avg;
    min = other.min;
    max = other.max;
    current = other.current;
    scale = other.scale;
    start_time = other.start_time;
    latest_time = other.latest_time;
  }

 public:  
  void setAvg(uint64_t avg) { this->avg = avg; }

  void setMax(uint64_t max) { this->max = max; }

  void setMin(uint64_t min) { this->min = min; }

  void setCurrent(uint64_t current) { this->current = current; }

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

 protected:
  std::string device_id;  

  Timestamp_t start_time;

  Timestamp_t latest_time;

  uint64_t avg;

  uint64_t min;

  uint64_t max;

  uint64_t current;

  uint64_t scale; 

};
