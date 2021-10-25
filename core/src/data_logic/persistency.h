#pragma once

#include <map>

#include "infrastructure/measurement_type.h"
#include "infrastructure/measurement_data.h"

namespace xpum {

class Persistency {
 public:
  virtual ~Persistency() {};

  virtual void storeMeasurementData(
      MeasurementType type,
      Timestamp_t time,
      std::map<std::string, MeasurementData>& datas) = 0;
      
};

} // end namespace xpum
