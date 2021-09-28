#pragma once

#include "const.h"
#include "persistency.h"
#include "raw_data_manager.h"
#include "data_logic_interface.h"
#include "xpum_structs.h"
#include <vector>

class DataLogic : public DataLogicInterface {
 public:
  DataLogic();

  virtual ~DataLogic();

  void init() override;

  void close() override;

  void storeMeasurementData(
      MeasurementType type, 
      Timestamp_t time,
      std::map<std::string, std::shared_ptr<MeasurementData>>& datas
  ) override;

  MeasurementData getLatestData(
    MeasurementType type,
    std::string& device_id
  ) override;

  void getLatestData(
    MeasurementType type,
    std::map<std::string, MeasurementData>& datas
  ) override;

  void getMetricsStatistics(xpum_device_id_t deviceId, 
                            xpum_device_stats_t dataList[], 
                            int *count,
                            uint64_t *begin,
                            uint64_t *end);

  uint32_t startRawDataCollectionTask(xpum_device_id_t device_id, std::vector<MeasurementType> types);

  void stopRawDataCollectionTask(uint32_t task_id);

  std::deque<MeasurementCacheData> getCachedRawData(uint32_t task_id, MeasurementType type);

  std::vector<std::deque<MeasurementCacheData>> getCachedRawData(uint32_t task_id);

private:
  std::unique_ptr<RawDataManager> p_raw_data_manager;

  std::shared_ptr<Persistency> p_persistency;

};


