#pragma once

#include <vector>
#include <mutex>
#include <string>

#include "infrastructure/const.h"
#include "infrastructure/property.h"
#include "infrastructure/device_capability.h"
#include "infrastructure/measurement_type.h"
#include "level_zero/ze_api.h"
#include "level_zero/zes_api.h"
#include "level_zero/zet_api.h"
#include "../include/xpum_structs.h"

namespace xpum {

/*
  Device class defines various interfaces for communication with devices.
*/

class Device {
 public:
  std::string getId() noexcept;

  void getCapability(std::vector<DeviceCapability>& capabilites) noexcept;
  
  bool hasCapability(DeviceCapability& cap) noexcept;

  void getProperties(std::vector<Property>& properties) noexcept;

  bool getProperty(xpum_device_property_name_t name, Property& ret) noexcept;

  virtual void getPower(Callback_t callback) noexcept = 0;
  
  virtual void getActuralFrequency(Callback_t callback) noexcept = 0;

  virtual void getRequestFrequency(Callback_t callback) noexcept = 0;

  virtual void getTemperature(Callback_t callback) noexcept = 0;

  virtual void getMemory(Callback_t callback) noexcept = 0;

  virtual void getMemoryUtilization(Callback_t callback) noexcept = 0;

  virtual void getMemoryBandwidth(Callback_t callback) noexcept = 0;

  virtual void getMemoryRead(Callback_t callback) noexcept = 0;

  virtual void getMemoryWrite(Callback_t callback) noexcept = 0;

  virtual void getEngineUtilization(Callback_t callback) noexcept = 0;

  virtual void getEngineGroupUtilization(Callback_t callback, zes_engine_group_t engine_group_type) noexcept = 0;

  virtual void getEnergy(Callback_t callback) noexcept = 0;

  virtual void getOccupationEfficiency(Callback_t callback, MeasurementType type) noexcept = 0;

  virtual void getRasError(Callback_t callback,const zes_ras_error_cat_t &rasCat, const zes_ras_error_type_t &rasType) noexcept = 0;

  void addCapability(DeviceCapability& capability);

  void removeCapability(DeviceCapability& capability); 

  void addProperty(Property prop);

  void removeProperty(xpum_device_property_name_t name);

  zes_device_handle_t getDeviceHandle();

  virtual bool runFirmwareFlash( const char* filePath, const std::string& toolPath ) noexcept;
  virtual xpum_firmware_flash_result_t getFirmwareFlashResult() noexcept;
  
  ze_device_handle_t getDeviceZeHandle();

  ze_driver_handle_t getDriverHandle();

  virtual bool isUpgradingFw( void ) noexcept;
  
 public:
  virtual ~Device() {}

 protected:
  std::string id;

  zes_device_handle_t zes_device_handle;

  ze_device_handle_t ze_device_handle;

  ze_driver_handle_t ze_driver_handle;

  std::mutex mutex;

  std::vector<DeviceCapability> capabilities;

  std::vector<Property> properties;

};

} // end namespace xpum
