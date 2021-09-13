
#pragma once

#include <map>
#include "xpum_structs.h"

class DataLogicQueryInterface {
 public:
  virtual ~DataLogicQueryInterface() {};

  virtual MeasurementData getLatestData(MeasurementType type,
                                        std::string& device_id) = 0;

  virtual void getLatestData(MeasurementType type,
                             std::map<std::string, MeasurementData>& datas) = 0;

  virtual void getMetricsStatistics(xpum_device_id_t deviceId, 
                                    xpum_device_stats_t dataList[], 
                                    int *count,
                                    uint64_t *begin,
                                    uint64_t *end) = 0;
};