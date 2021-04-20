
#pragma once

#include <map>

class DataLogicQueryInterface {
 public:
  virtual ~DataLogicQueryInterface() {};

  virtual MeasurementData getLatestData(MeasurementType type,
                                        std::string& device_id) = 0;

  virtual void getLatestData(MeasurementType type,
                             std::map<std::string, MeasurementData>& datas) = 0;
};