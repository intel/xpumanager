#pragma once

#include "measurement_type.h"
#include "persistency.h"

class DBPersistency : public Persistency {
 public:
  virtual void storeMeasurementData(
      MeasurementType type,
      Timestamp_t time,
      std::map<std::string, MeasurementData>& datas) override;
      
};
