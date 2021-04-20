#pragma once

#include <atomic>

#include "base_exception.h"
#include "const.h"
#include "shared_data.h"
#include "shared_queue.h"
#include "persistency.h"
#include "measurement_type.h"

class DataHandler : public std::enable_shared_from_this<DataHandler> {
 public:
  DataHandler(MeasurementType type, std::shared_ptr<Persistency>& p_persistency);

  virtual ~DataHandler();

  void init();

  void close(); 

  void handleData(std::shared_ptr<SharedData>& p_data) noexcept;
  
  MeasurementData getLatestData(std::string& device_id) noexcept;

  void getLatestData(std::map<std::string, MeasurementData>& datas) noexcept;

 private:
  MeasurementType type;

  SharedQueue<std::shared_ptr<SharedData>> q;

  std::atomic<bool> stop;

  std::shared_ptr<SharedData> p_latestData; 

  std::shared_ptr<Persistency> p_persistency;

  std::mutex  mutex;
  
};
