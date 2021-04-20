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

  MeasurementData(int value): avg(value), 
    min(value), max(value), current(value), scale(1) {
  }  

  MeasurementData(int value, int scale): avg(value), 
    min(value), max(value), current(value), scale(scale) {
  }

  MeasurementData(int avg, int min, int max): avg(avg), 
    min(min), max(max), current(-1), scale(1) {
  }

  MeasurementData(int avg, int min, int max, int current, int scale): 
    avg(avg), min(min), max(max), current(current), scale(scale) {
  }

  MeasurementData(const MeasurementData& other) {
    avg = other.avg;
    min = other.min;
    max = other.max;
    current = other.current;
    scale = other.scale;
  }

 public:  
  void setAvg(int avg) { this->avg = avg; }

  void setMax(int max) { this->max = max; }

  void setMin(int min) { this->min = min; }

  void setCurrent(int current) { this->current = current; }

  void setScale(int scale) { this->scale = scale; }

  void setDeviceId(const std::string& device_id) { this->device_id = scale; }

  int getAvg() { return this->avg; }

  int getMax() { return this->max; }

  int getMin() { return this->min; }

  int getCurrent() { return this->current; }

  int getScale() { return this->scale; }

  std::string getDeviceId() { return this->device_id; }

 protected:
  std::string device_id;  

  Timestamp_t time;

  int avg;

  int min;

  int max;

  int current;

  int scale; 

};
