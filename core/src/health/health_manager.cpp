#include "logger.h"
#include "health_manager.h"
#include "gpu_device_stub.h"
#include "configuration.h"

HealthManager::HealthManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                               std::shared_ptr<DataLogicInterface>& p_data_logic)
    : p_device_manager(p_device_manager), p_data_logic(p_data_logic) {    
  LOG_INFO("HealthManager()");
}

HealthManager::~HealthManager() {
  LOG_INFO("~HealthManager()");
}

void HealthManager::init() {

}

void HealthManager::close() {

}

xpum_result_t HealthManager::setHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void *value) {
  std::unique_lock<std::mutex> lock(this->mutex);
  if (value == nullptr || *static_cast<int*>(value) == -1) {
    switch(key) {
        case xpum_health_config_type_t::XPUM_HEALTH_THEARMAL_LIMIT:
          p_health_thermal_configs.erase(deviceId);
          break;
        case xpum_health_config_type_t::XPUM_HEALTH_POWER_LIMIT:
          p_health_power_configs.erase(deviceId);
          break;
      }
    return XPUM_OK;
  }

  int threshold = *static_cast<int*>(value);
  switch(key) {
    case xpum_health_config_type_t::XPUM_HEALTH_THEARMAL_LIMIT:
      if (threshold <= 0 || threshold >= 105)
        return XPUM_GENERIC_ERROR;
      p_health_thermal_configs[deviceId] = threshold;   
      break;
    case xpum_health_config_type_t::XPUM_HEALTH_POWER_LIMIT:
      if (threshold <= 0)
        return XPUM_GENERIC_ERROR;
      p_health_power_configs[deviceId] = threshold;
  }
  return XPUM_OK;
}


xpum_result_t HealthManager::getHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void *value) {
  std::unique_lock<std::mutex> lock(this->mutex);
  if (value == nullptr) {
    return XPUM_GENERIC_ERROR;
  }

  int* threshold = static_cast<int*>(value);
  *threshold = -1;

  switch(key) {
    case xpum_health_config_type_t::XPUM_HEALTH_THEARMAL_LIMIT:
      if (p_health_thermal_configs.find(deviceId) != p_health_thermal_configs.end()) {
        *threshold = p_health_thermal_configs.at(deviceId);
      }
      break;
    case xpum_health_config_type_t::XPUM_HEALTH_POWER_LIMIT:
      if (p_health_power_configs.find(deviceId) != p_health_power_configs.end()) {
        *threshold = p_health_power_configs.at(deviceId);
      }
      break;
  }
  return XPUM_OK;
}


xpum_result_t HealthManager::getHealth(xpum_device_id_t deviceId, xpum_health_type_t type, xpum_health_data_t *data) {
  std::unique_lock<std::mutex> lock(this->mutex);
  data->deviceId = deviceId;
  data->type = type;
  data->status = xpum_health_status_t::XPUM_HEALTH_STATUS_UNKNOWN;
  
  bool global_default_limit = true;
  int thermal_thresold = Configuration::TEMPERATURE_HEALTH_DEFAULT_LIMIT;
  if (p_health_thermal_configs.find(deviceId) != p_health_thermal_configs.end()) {
    thermal_thresold = p_health_thermal_configs.at(deviceId);
    global_default_limit = false;
  }

  int power_threshold = Configuration::POWER_HEALTH_DEFAULT_LIMIT;
  if (p_health_power_configs.find(deviceId) != p_health_power_configs.end()) {
    power_threshold = p_health_power_configs.at(deviceId);
    global_default_limit = false;
  }

  GPUDeviceStub::instance().getHealthStatus(
    this->p_device_manager->getDevice(std::to_string(deviceId))->getDeviceHandle(), type, data, thermal_thresold, power_threshold, global_default_limit);
  
  return XPUM_OK;
}