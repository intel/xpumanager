#pragma once

#include <vector>

#include "monitor_manager_interface.h"
#include "monitor_task.h"

class MonitorManager : public MonitorManagerInterface {
 public:
  MonitorManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                 std::shared_ptr<DataLogicInterface>& p_data_logic);

  virtual ~MonitorManager();

 public:
  void init() override;

  void close() override;

 private:
  void createMonitorTasks();

 private:
  std::shared_ptr<DeviceManagerInterface> p_device_manager;

  std::shared_ptr<DataLogicInterface> p_data_logic;

  std::vector<std::shared_ptr<MonitorTask>> tasks;
  
  std::mutex mutex;
};
