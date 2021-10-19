#pragma once

#include "data_handler.h"

struct Statistics_subdevice_data {
  uint64_t count;
  uint64_t avg;
  uint64_t min;
  uint64_t max;
  Statistics_subdevice_data(uint64_t data) {
    min = data;
    max = data;
    avg = data;
    count = 1;
  }
};

struct Statistics_data {
  uint64_t count;
  uint64_t avg;
  uint64_t min;
  uint64_t max;
  long long start_time;
  long long latest_time;
  bool hasDataOnDevice;
  std::map<uint32_t, Statistics_subdevice_data> subdevice_datas;
  Statistics_data(uint64_t data, long long time) {
      min = data;
      max = data;
      avg = data;
      count = 1;
      start_time = time;
      latest_time = time;
  }
};

class MetricStatisticsDataHandler : public DataHandler {
public:
  MetricStatisticsDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

  virtual ~MetricStatisticsDataHandler();

  virtual void handleData(std::shared_ptr<SharedData> &p_data) noexcept;

  virtual MeasurementData getLatestData(std::string &device_id) noexcept;

  virtual MeasurementData getLatestStatistics(std::string &device_id) noexcept;

protected:

  void resetStatistics(std::string &device_id);

  void updateStatistics(std::shared_ptr<SharedData> &p_data);

  std::map<std::string, Statistics_data> statistics_datas;

};