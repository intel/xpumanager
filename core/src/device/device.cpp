#include <cstring>

#include "infrastructure/exception/ilegal_parameter_exception.h"
#include "device.h"

std::string Device::getId() noexcept {
  std::unique_lock<std::mutex> lock(this->mutex);
  return id;
}

void Device::getCapability(std::vector<DeviceCapability>& capabilites) noexcept {
  std::unique_lock<std::mutex> lock(this->mutex);
  for (auto& cap : capabilities) {
    capabilites.push_back(cap);
  }
}

bool Device::hasCapability(DeviceCapability& cap) noexcept {
  std::unique_lock<std::mutex> lock(this->mutex);
  for (auto& existing_cap : capabilities) {
    if (existing_cap == cap) {
      return true;
    }
  }
  return false;
}
 
void Device::getProperties(std::vector<Property>& properties) noexcept {
  std::unique_lock<std::mutex> lock(this->mutex);
  for (auto& prop : this->properties) {
    properties.push_back(prop);
  }
}

bool Device::getProperty(const std::string& name, Property& ret) noexcept {
  std::unique_lock<std::mutex> lock(this->mutex);
  for (auto& prop : this->properties) {
    if (prop.getName() == name) {
      ret.setValue(prop.getValue());
      return true;
    }
  }

  return false;  
}

void Device::addCapability(DeviceCapability& capability) {
  std::unique_lock<std::mutex> lock(this->mutex);
  for (auto& cap : capabilities) {
    if (cap == capability) {
      return ;
    }
  }

  this->capabilities.push_back(capability);
}

void Device::removeCapability(DeviceCapability& capability) {
  std::unique_lock<std::mutex> lock(this->mutex);
  for (auto it = capabilities.begin(); it != capabilities.end(); ++it) {
    if (*it == capability) {
      capabilities.erase(it);
      return ;
    }
  }
}

void Device::addProperty(Property prop) {
  std::unique_lock<std::mutex> lock(this->mutex);
  for (auto it = properties.begin(); it != properties.end(); ++it) {
    if (prop.getName() == it->getName()) {
      it->setValue(prop.getValue());
      return ;
    }
  }

  this->properties.push_back(prop);
}

void Device::removeProperty(const std::string& name) {
  std::unique_lock<std::mutex> lock(this->mutex);
  for (auto it = properties.begin(); it != properties.end(); ++it) {
    if (it->getName() == name) {
      properties.erase(it);
      return ;
    }
  }
}

zes_device_handle_t Device::getDeviceHandle() {
  std::unique_lock<std::mutex> lock(this->mutex);
  return zes_device_handle;
}

bool Device::runFirmwareFlash( const char* filePath, const std::string& toolPath ) noexcept  {
  return false;
}

xpum_firmware_flash_result_t Device::getFirmwareFlashResult() noexcept {
  return XPUM_DEVICE_FIRMWARE_FLASH_OK;
}

ze_device_handle_t Device::getDeviceZeHandle() {
  std::unique_lock<std::mutex> lock(this->mutex);
  return ze_device_handle;
}

ze_driver_handle_t Device::getDriverHandle() {
  std::unique_lock<std::mutex> lock(this->mutex);
  return ze_driver_handle;
}

bool Device::isUpgradingFw( void ) noexcept {
  return false;
}
