#include "device_capability.h"
#include "configuration.h"
#include "monitor_task.h"
#include "logger.h"
#include "monitor_manager.h"

MonitorManager::MonitorManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                               std::shared_ptr<DataLogicInterface>& p_data_logic)
    : p_device_manager(p_device_manager), p_data_logic(p_data_logic) {    
  Logger::instance().info("MonitorManager()");
}

MonitorManager::~MonitorManager() {
  Logger::instance().info("~MonitorManager()");
}

void MonitorManager::init() {
  std::unique_lock<std::mutex> lock(this->mutex);

  createMonitorTasks();  

  for (auto& p_task : tasks) {
    p_task->start();
  }
}

void MonitorManager::close() {
  std::unique_lock<std::mutex> lock(this->mutex);
  for (auto& p_task : tasks) {
    p_task->stop();
  }  
}

void MonitorManager::createMonitorTasks() {
  tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::POWER, 
    Configuration::POWER_MONITOR_FREQUENCE, p_device_manager, p_data_logic));
  tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::FREQUENCY, 
    Configuration::POWER_MONITOR_FREQUENCE, p_device_manager, p_data_logic));
  tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::TEMPERATURE, 
    Configuration::POWER_MONITOR_FREQUENCE, p_device_manager, p_data_logic));
}

