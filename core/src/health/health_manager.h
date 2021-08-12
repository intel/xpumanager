#pragma once

#include <vector>

#include "health_manager_interface.h"
#include "device_manager_interface.h"
#include "data_logic_interface.h"
#include "health_data_type.h"

class HealthManager : public HealthManagerInterface {
 public:
  HealthManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                 std::shared_ptr<DataLogicInterface>& p_data_logic);

  virtual ~HealthManager();

 public:
  void init() override;

  void close() override;

  void getDeviceHealthStatus(const std::string& id, HealthType& type, HealthStatus& status, std::string& description) override;

 private:
  std::shared_ptr<DeviceManagerInterface> p_device_manager;

  std::shared_ptr<DataLogicInterface> p_data_logic;
  
  std::mutex mutex;
};
