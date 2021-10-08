#pragma once

#include "health_manager_interface.h"
#include "control/device_manager_interface.h"
#include "data_logic/data_logic_interface.h"
#include "health_data_type.h"

class HealthManager : public HealthManagerInterface {
 public:
  HealthManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                 std::shared_ptr<DataLogicInterface>& p_data_logic);

  virtual ~HealthManager();

 public:
  void init() override;

  void close() override;

  xpum_result_t setHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void *value) override;

  xpum_result_t getHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void *value) override;

  xpum_result_t getHealth(xpum_device_id_t deviceId, xpum_health_type_t type, xpum_health_data_t *data) override;

 private:
  std::shared_ptr<DeviceManagerInterface> p_device_manager;

  std::shared_ptr<DataLogicInterface> p_data_logic;

  std::map<xpum_device_id_t, int> p_health_thermal_configs;

  std::map<xpum_device_id_t, int> p_health_power_configs;
  
  std::mutex mutex;
};
