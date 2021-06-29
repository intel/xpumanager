#pragma once

#include <string>
#include "const.h"
#include "device.h"
#include "measurement_data.h"
#include "thread_pool.h"
#include "ze_api.h"
#include "zes_api.h"

class GPUDeviceStub {
 public:
  static GPUDeviceStub& instance();

 public:  
  void discoverDevices(Callback_t callback);
  
  void getPower(const std::string& device_id, Callback_t callback) noexcept;

  void getActuralFrequency(const std::string& device_id, Callback_t callback) noexcept;

  void getTemperature(const std::string& device_id, Callback_t callback) noexcept;

private:
  GPUDeviceStub();

  ~GPUDeviceStub();

  GPUDeviceStub& operator=(const GPUDeviceStub &) = delete;

  GPUDeviceStub(const GPUDeviceStub &other) = delete;

  void init();

  static std::shared_ptr<std::vector<std::shared_ptr<Device>>> toDiscover();

  static std::shared_ptr<MeasurementData> toGetPower(const std::string& device_id);

  static std::shared_ptr<MeasurementData> toGetActuralFrequency(const std::string& device_id);

  static std::shared_ptr<MeasurementData> toGetTemperature(const std::string& device_id);

  static std::string to_string(ze_device_uuid_t val);

  static std::string to_hex_string(uint32_t val);

  static std::string get_health_state_string(zes_mem_health_t val);

  static std::string to_string(zes_pci_address_t address);

  static zes_device_handle_t getDeviceById(std::string Id);

 private:
  std::unique_ptr<ThreadPool>  p_thread_pool;
  
  bool initialized;

  std::vector<zes_device_handle_t> zes_devices;
  
  std::mutex  mutex;
    
};
