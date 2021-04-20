#pragma once

#include <map>
#include "const.h"

#include "measurement_type.h"
#include "measurement_data.h"

class DataLogicPersistenceInterface {
 public:
  virtual ~DataLogicPersistenceInterface() {}
  
  virtual void storeMeasurementData(
      MeasurementType type, Timestamp_t time,
      std::map<std::string, std::shared_ptr<MeasurementData>>& datas) = 0;
};
