#pragma once

#include <vector>
#include <string>

#include "infrastructure/init_close_interface.h"
#include "health_data_type.h"

class HealthManagerInterface : public InitCloseInterface {
 public:
  virtual ~HealthManagerInterface() {}

  virtual xpum_result_t setHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void *value) = 0;

  virtual xpum_result_t getHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void *value) = 0;

  virtual xpum_result_t getHealth(xpum_device_id_t deviceId, xpum_health_type_t type, xpum_health_data_t *data) = 0;
  
};