#pragma once

#include <map>

#include "infrastructure/const.h"
#include "infrastructure/measurement_data.h"

class SharedData {
 public: 
  SharedData(Timestamp_t time, std::map<std::string, std::shared_ptr<MeasurementData>>& datas);

  virtual ~SharedData();

 public:
  std::map<std::string, MeasurementData>& getData() noexcept;

  Timestamp_t getTime() noexcept;

 private:
  Timestamp_t time;

  std::map<std::string, MeasurementData> datas;  

};
