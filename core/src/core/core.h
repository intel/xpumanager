#pragma once

#include <string>

#include "data_logic_interface.h"
#include "device_manager_interface.h"
#include "monitor_manager_interface.h"

class Core : public InitCloseInterface {
 public:
  static Core& instance();

  void init() override;

  void close() override;
  
  bool isInitialized();

  std::shared_ptr<DeviceManagerInterface> getDeviceManager();

  std::shared_ptr<DataLogicInterface> getDataLogic();

  std::shared_ptr<MonitorManagerInterface> getMonitorManager();

 private:
  Core();

  Core& operator=(const Core &) = delete;

  Core(const Core &other) = delete;

  ~Core();

  void close(const std::shared_ptr<InitCloseInterface>& p_init_close_interface, const std::string& p_msgPrix);

 private:
  std::shared_ptr<DeviceManagerInterface> p_device_manager;

  std::shared_ptr<DataLogicInterface> p_data_logic;

  std::shared_ptr<MonitorManagerInterface> p_monitor_manager;

  bool initialized;

  std::mutex mutex;
};
