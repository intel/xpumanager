#pragma once

#include "device.h"

class GPUDevice : public Device {
 public:
  GPUDevice();
  GPUDevice(const std::string& id, std::vector<DeviceCapability>& capabilities);

  virtual ~GPUDevice();

 public:
  void getPower(Callback_t callback) noexcept override;
  void getActuralFrequency(Callback_t callback) noexcept override;
  void getTemperature(Callback_t callback) noexcept override;
  void getMemory(Callback_t callback) noexcept override;
  void getEngineUtilization(Callback_t callback) noexcept override;

};