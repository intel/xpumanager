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
  DataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

  virtual ~DataHandler();

  void init();

  void close();

  virtual void handleData(std::shared_ptr<SharedData> &p_data) noexcept;

  virtual MeasurementData getLatestData(std::string &device_id) noexcept;

  virtual void getLatestData(std::map<std::string, MeasurementData> &datas) noexcept;

protected:
  std::mutex mutex;

  std::shared_ptr<SharedData> p_latestData;

  SharedQueue<std::shared_ptr<SharedData>> q;

private:
  MeasurementType type;

  std::atomic<bool> stop;

  std::shared_ptr<Persistency> p_persistency;
};
