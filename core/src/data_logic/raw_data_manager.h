#pragma once

#include <map>
#include <mutex>

#include "persistency.h"
#include "data_handler.h"

class RawDataManager {
 public:
  RawDataManager(std::shared_ptr<Persistency>& persistency);

  virtual ~RawDataManager();

  void init();

  void close();

  void storeMeasurementData(
      MeasurementType type, 
      Timestamp_t time,
      std::map<std::string, std::shared_ptr<MeasurementData>>& datas
  );

  MeasurementData getLatestData(
    MeasurementType type,
    std::string& device_id
  ) noexcept;

  void getLatestData(
    MeasurementType type,
    std::map<std::string, MeasurementData>& datas
    ) noexcept;

 private:
  RawDataManager() = default;

 private:
  std::map<MeasurementType, std::shared_ptr<DataHandler>> data_handlers;

  std::shared_ptr<Persistency> p_persistency;

  std::mutex  mutex;

};
