#include "logger.h"
#include "health_manager.h"
#include "gpu_device_stub.h"
#include "configuration.h"
#include <iostream>

HealthManager::HealthManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                               std::shared_ptr<DataLogicInterface>& p_data_logic)
    : p_device_manager(p_device_manager), p_data_logic(p_data_logic) {    
  Logger::instance().info("HealthManager()");
}

HealthManager::~HealthManager() {
  Logger::instance().info("~HealthManager()");
}

void HealthManager::init() {

}

void HealthManager::close() {

}

void HealthManager::getDeviceHealthStatus(const std::string& id, HealthType& type, HealthStatus& status, std::string& description) {
    std::unique_lock<std::mutex> lock(this->mutex);
    GPUDeviceStub::instance().getHealthStatus(this->p_device_manager->getDevice(id)->getDeviceHandle(), type, status, description);
}