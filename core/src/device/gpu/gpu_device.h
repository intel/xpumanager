#pragma once

#include "device/device.h"

#include "stdio.h"
#include <mutex>

class GPUDevice : public Device {
 public:
  GPUDevice();
  GPUDevice(const std::string& id, const zes_device_handle_t& zes_device, std::vector<DeviceCapability>& capabilities);
  GPUDevice(const std::string& id, const zes_device_handle_t& zes_device, const ze_device_handle_t& ze_device, const ze_driver_handle_t& driver, std::vector<DeviceCapability>& capabilities);

  virtual ~GPUDevice();

 public:
  void getPower(Callback_t callback) noexcept override;
  void getActuralFrequency(Callback_t callback) noexcept override;
  void getTemperature(Callback_t callback) noexcept override;
  void getMemory(Callback_t callback) noexcept override;
  void getMemoryUtilization(Callback_t callback) noexcept override;
  void getMemoryBandwidth(Callback_t callback) noexcept override;
  void getMemoryRead(Callback_t callback) noexcept override;
  void getMemoryWrite(Callback_t callback) noexcept override;
  void getEngineUtilization(Callback_t callback) noexcept override;
  void getEngineGroupUtilization(Callback_t callback, zes_engine_group_t engine_group_type) noexcept override;
  void getEnergy(Callback_t callback) noexcept override;
  void getOccupationEfficiency(Callback_t callback, MeasurementType type) noexcept override;
  void getRasError(Callback_t callback,const zes_ras_error_cat_t &rasCat, const zes_ras_error_type_t &rasType)  noexcept override;

  virtual bool runFirmwareFlash( const char* filePath, const std::string& toolPath ) noexcept override;
  virtual xpum_firmware_flash_result_t getFirmwareFlashResult() noexcept override;

  virtual bool isUpgradingFw( void ) noexcept override;

 private:
  void dumpFirmwareFlashLog() noexcept;

 private:
  FILE* commandExec;
  std::mutex mtx;
  std::string log;

  static const unsigned int BUFFERSIZE = 64 * 1024;
  static const std::string logFilePath;
};
