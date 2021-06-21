#pragma once

#include "data_handler.h"

class StatisticsDataHandler : public DataHandler {
public:
  StatisticsDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

  virtual ~StatisticsDataHandler();

  virtual void handleData(std::shared_ptr<SharedData> &p_data) noexcept;

  virtual MeasurementData getLatestData(std::string &device_id) noexcept;

  virtual void getLatestData(std::map<std::string, MeasurementData> &datas) noexcept;

  void getCacheMinMaxAvg(std::string& device_id, int& min, int& max, int& avg);

protected:
  std::deque<std::shared_ptr<SharedData>> cache;
};