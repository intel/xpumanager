#pragma once

#include "const.h"
#include "persistency.h"
#include "raw_data_manager.h"
#include "data_logic_interface.h"

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

 private:
  std::unique_ptr<RawDataManager> p_raw_data_manager;

  std::shared_ptr<Persistency> p_persistency;

};


