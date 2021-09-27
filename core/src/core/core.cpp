#include "logger.h"
#include "configuration.h"
#include "ilegal_state_exception.h"
#include "data_logic.h"
#include "device_manager.h"
#include "monitor_manager.h"
#include "health_manager.h"
#include "diagnostic_manager.h"
#include "policy_manager.h"
#include "core.h"
#include "group_manager.h"

Core::Core()
    : p_device_manager(nullptr),
      p_data_logic(nullptr),
      p_monitor_manager(nullptr),
      p_health_manager(nullptr),
      p_diagnostic_manager(nullptr),
      p_policy_manager(nullptr),
      initialized(false) {
    Logger::init();
    LOG_INFO("core()");
}

Core::~Core() {
    LOG_INFO("~core()");
    close();
}

Core& Core::instance() {
  static Core instance;
  return instance;
}

std::shared_ptr<DeviceManagerInterface> Core::getDeviceManager() {
  std::unique_lock<std::mutex> lock(mutex);
  return p_device_manager;
}

std::shared_ptr<DataLogicInterface> Core::getDataLogic() {
  std::unique_lock<std::mutex> lock(mutex);
  return p_data_logic;
}

std::shared_ptr<MonitorManagerInterface> Core::getMonitorManager() {
  std::unique_lock<std::mutex> lock(mutex);
  return p_monitor_manager;
}

std::shared_ptr<HealthManagerInterface> Core::getHealthManager() {
  std::unique_lock<std::mutex> lock(mutex);
  return p_health_manager;
}

std::shared_ptr<GroupManagerInterface> Core::getGroupManager(){
  std::unique_lock<std::mutex> lock(mutex);
  return p_group_manager;  
}

std::shared_ptr<DiagnosticManagerInterface> Core::getDiagnosticManager() {
  std::unique_lock<std::mutex> lock(mutex);
  return p_diagnostic_manager;
}

std::shared_ptr<PolicyManagerInterface> Core::getPolicyManager() {
  std::unique_lock<std::mutex> lock(mutex);
  return p_policy_manager;
}

void Core::init() {
  std::unique_lock<std::mutex> lock(mutex);
  if (initialized)  {
    return ;
  }

  LOG_INFO("initialize configuration");
  Configuration::init();

  LOG_INFO("initialize datalogic");
  p_data_logic = std::make_shared<DataLogic>();
  p_data_logic->init();

  LOG_INFO("initialize device manager");
  p_device_manager = std::make_shared<DeviceManager>(p_data_logic);
  p_device_manager->init();

  LOG_INFO("initialize monitor manager");
  p_monitor_manager = std::make_shared<MonitorManager>(p_device_manager, p_data_logic);
  p_monitor_manager->init();

  LOG_INFO("initialize health manager");
  p_health_manager = std::make_shared<HealthManager>(p_device_manager, p_data_logic);
  p_health_manager->init();

  LOG_INFO("initialize group manager");
  p_group_manager = std::make_shared<GroupManager>(p_device_manager, p_data_logic);
  p_group_manager->init();

  LOG_INFO("initialize diagnostic manager");
  p_diagnostic_manager = std::make_shared<DiagnosticManager>(p_device_manager, p_data_logic);
  p_diagnostic_manager->init();

  LOG_INFO("initialize policy manager");
  p_policy_manager = std::make_shared<PolicyManager>(p_device_manager, p_data_logic,p_group_manager);
  p_policy_manager->init();

  initialized = true;  
}

void Core::close() {
  std::unique_lock<std::mutex> lock(mutex);
  if (!initialized)  {
    return ;
  }  

  close(std::dynamic_pointer_cast<InitCloseInterface>(p_policy_manager), 
    "Failed to close policy manager");
  close(std::dynamic_pointer_cast<InitCloseInterface>(p_diagnostic_manager), 
    "Failed to close diagnostic manager");
  close(std::dynamic_pointer_cast<InitCloseInterface>(p_group_manager), 
    "Failed to close group manager");
  close(std::dynamic_pointer_cast<InitCloseInterface>(p_health_manager), 
    "Failed to close health manager");
  close(std::dynamic_pointer_cast<InitCloseInterface>(p_monitor_manager),
    "Failed to close monitor manager");
  close(std::dynamic_pointer_cast<InitCloseInterface>(p_device_manager),
    "Failed to close device manager");
  close(std::dynamic_pointer_cast<InitCloseInterface>(p_data_logic), 
    "Failed to close data logic");
}

void Core::close(const std::shared_ptr<InitCloseInterface>& p_init_close_interface,
                 const std::string& p_msgPrix) {
  if (p_init_close_interface == nullptr) {
    return;
  }

  try {
    p_init_close_interface->close();
  } catch (std::exception& e) {  
    LOG_WARN("{}: {}", p_msgPrix, e.what());    
  } catch (...) {
    LOG_WARN("{}: unexpected exception", p_msgPrix);    
  }
}

bool Core::isInitialized() {
  std::unique_lock<std::mutex> lock(mutex);
  return this->initialized;  
}

