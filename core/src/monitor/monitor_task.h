#pragma once

#include "timer.h"
#include "device_capability.h"
#include "device_manager_interface.h"
#include "data_logic_interface.h"

class MonitorTask : public std::enable_shared_from_this<MonitorTask> {
 public:
  MonitorTask(
    DeviceCapability capability, 
    int freq,
    std::shared_ptr<DeviceManagerInterface>& p_device_manager,
    std::shared_ptr<DataLogicInterface>& p_data_logic
  );

  virtual ~MonitorTask();

 public:
  void start();

  void stop();
 
 private:
  DeviceCapability capability;

  int freq;

  Timer timer;

  std::mutex mutex;

  std::condition_variable data_cv;  

  std::shared_ptr<DeviceManagerInterface> p_device_manager;

  std::shared_ptr<DataLogicInterface> p_data_logic;

};

