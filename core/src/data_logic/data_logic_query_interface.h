
#pragma once

#include <deque>
#include <map>

#include "../include/xpum_structs.h"
#include "infrastructure/measurement_cache_data.h"

namespace xpum {

class DataLogicQueryInterface {
   public:
    virtual ~DataLogicQueryInterface(){};

    virtual MeasurementData getLatestData(MeasurementType type,
                                          std::string &device_id) = 0;

    virtual void getLatestData(MeasurementType type,
                               std::map<std::string, MeasurementData> &datas) = 0;

    virtual MeasurementData getLatestStatistics(MeasurementType type, std::string &device_id) = 0;

    virtual void getMetricsStatistics(xpum_device_id_t deviceId,
                                      xpum_device_stats_t dataList[],
                                      int *count,
                                      uint64_t *begin,
                                      uint64_t *end) = 0;

    virtual void getLatestMetrics(xpum_device_id_t deviceId,
                                  xpum_device_metrics_t dataList[],
                                  int *count) = 0;

    virtual uint32_t startRawDataCollectionTask(xpum_device_id_t device_id, std::vector<MeasurementType> types) = 0;

    virtual void stopRawDataCollectionTask(uint32_t task_id) = 0;

    virtual std::deque<MeasurementCacheData> getCachedRawData(uint32_t task_id, MeasurementType type) = 0;

    virtual std::vector<std::deque<MeasurementCacheData>> getCachedRawData(uint32_t task_id) = 0;
};
} // end namespace xpum
