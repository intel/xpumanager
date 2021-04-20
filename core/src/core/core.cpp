#include "logger.h"
#include "configuration.h"
#include "ilegal_state_exception.h"
#include "data_logic.h"
#include "device_manager.h"
#include "monitor_manager.h"
#include "core.h"

Core::Core()
    : p_device_manager(nullptr),
      p_data_logic(nullptr),
      p_monitor_manager(nullptr),
      initialized(false) {
  Logger::instance().info("core()");
}

Core::~Core() {
  Logger::instance().info("~core()");
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

void Core::init() {
  std::unique_lock<std::mutex> lock(mutex);
  if (initialized)  {
    return ;
  }

  Logger::instance().info("initialize configuration");
  Configuration::init();

  Logger::instance().info("initialize datalogic");
  p_data_logic = std::make_shared<DataLogic>();
  p_data_logic->init();

  Logger::instance().info("initialize device manager");
  p_device_manager = std::make_shared<DeviceManager>(p_data_logic);
  p_device_manager->init();

  Logger::instance().info("initialize monitor manager");
  p_monitor_manager = std::make_shared<MonitorManager>(p_device_manager, p_data_logic);
  p_monitor_manager->init();

  initialized = true;  
}

void Core::close() {
  std::unique_lock<std::mutex> lock(mutex);
  if (!initialized)  {
    return ;
  }  

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
    std::string error_msg = p_msgPrix;
    error_msg += ": ";
    error_msg += e.what();
    Logger::instance().warn(error_msg);    
  } catch (...) {
    std::string error_msg = p_msgPrix;
    error_msg += ": unexpected exception";
    Logger::instance().warn(error_msg);
  }
}

bool Core::isInitialized() {
  std::unique_lock<std::mutex> lock(mutex);
  return this->initialized;  
}

