#pragma once

#include <vector>
#include <mutex>
#include <string>

#include "const.h"
#include "property.h"
#include "device_capability.h"
#include "ze_api.h"
#include "zes_api.h"

class Device {
 public:
  std::string getId() noexcept;

  void getCapability(std::vector<DeviceCapability>& capabilites) noexcept;
  
  bool hasCapability(DeviceCapability& cap) noexcept;

  void getProperties(std::vector<Property>& properties) noexcept;

  bool getProperty(const std::string& name, Property& ret) noexcept;

  virtual void getPower(Callback_t callback) noexcept = 0;
  
  virtual void getActuralFrequency(Callback_t callback) noexcept = 0;

  virtual void getTemperature(Callback_t callback) noexcept = 0;

  virtual void getMemory(Callback_t callback) noexcept = 0;

  virtual void getEngineUtilization(Callback_t callback) noexcept = 0;

  void addCapability(DeviceCapability& capability);

  void removeCapability(DeviceCapability& capability); 

  void addProperty(Property prop);

  void removeProperty(const std::string& name);

  zes_device_handle_t getDeviceHandle();
  
 public:
  virtual ~Device() {}

 protected:
  std::string id;

  zes_device_handle_t zes_device_handle;

  std::mutex mutex;

  std::vector<DeviceCapability> capabilities;

  std::vector<Property> properties;

};
