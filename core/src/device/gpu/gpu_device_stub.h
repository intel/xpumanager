#pragma once

#include <string>
#include "const.h"
#include "device.h"
#include "measurement_data.h"
#include "thread_pool.h"
#include "ze_api.h"
#include "zes_api.h"
#include "scheduler.h"
#include "standby.h"
#include "power.h"
#include "frequency.h"
#include "health_data_type.h"

class GPUDeviceStub {
 public:
  static GPUDeviceStub& instance();

 public:  
  void discoverDevices(Callback_t callback);
  
  void getPower(const zes_device_handle_t& device, Callback_t callback) noexcept;

  void getActuralFrequency(const zes_device_handle_t& device, Callback_t callback) noexcept;

  void getTemperature(const zes_device_handle_t& device, Callback_t callback) noexcept;

  void getMemory(const zes_device_handle_t& device, Callback_t callback) noexcept;

  void getEngineUtilization(const zes_device_handle_t& device, Callback_t callback) noexcept;

  static void getSchedulers(const zes_device_handle_t& device, std::vector<Scheduler>& schedulers);

  static void getStandbys(const zes_device_handle_t& device, std::vector<Standby>& standbys);

  static void getPowerProps(const zes_device_handle_t& device, std::vector<Power>& powers);

  static void getPowerLimits(const zes_device_handle_t& device, 
                             Power_sustained_limit_t& sustained_limit,
                             Power_burst_limit_t& burst_limit,
                             Power_peak_limit_t& peak_limit);

  static bool setPowerSustainedLimits(const zes_device_handle_t& device, 
                                      const Power_sustained_limit_t& sustained_limit);

  static bool setPowerBurstLimits(const zes_device_handle_t& device, 
                                  const Power_burst_limit_t& burst_limit);

  static bool setPowerPeakLimits(const zes_device_handle_t& device, 
                                 const Power_peak_limit_t& peak_limit);

  static void getFrequencyRanges(const zes_device_handle_t& device,
                                 std::vector<Frequency>& frequencies);
  
  static bool setFrequencyRange(const zes_device_handle_t& device,
                                const Frequency& freq);

  static bool setStandby(const zes_device_handle_t& device, 
                         const Standby& standby); 
  
  static bool setSchedulerTimeoutMode(const zes_device_handle_t& device, 
                                      const SchedulerTimeoutMode& mode);

  static bool setSchedulerTimesliceMode(const zes_device_handle_t& device,
                                        const SchedulerTimesliceMode& mode);

  static bool setSchedulerExclusiveMode(const zes_device_handle_t& device,
                                        const SchedulerExclusiveMode& mode);

  static void getHealthStatus(const zes_device_handle_t& device, HealthType& type, HealthStatus& status, std::string& description);

private:
  GPUDeviceStub();

  ~GPUDeviceStub();

  GPUDeviceStub& operator=(const GPUDeviceStub &) = delete;

  GPUDeviceStub(const GPUDeviceStub &other) = delete;

  void init();

  static std::shared_ptr<std::vector<std::shared_ptr<Device>>> toDiscover();

  static std::shared_ptr<MeasurementData> toGetPower(const zes_device_handle_t& device);

  static std::shared_ptr<MeasurementData> toGetActuralFrequency(const zes_device_handle_t& device);

  static std::shared_ptr<MeasurementData> toGetTemperature(const zes_device_handle_t& device);

  static std::shared_ptr<MeasurementData> toGetMemory(const zes_device_handle_t& device);

  static std::shared_ptr<MeasurementData> toGetEngineUtilization(const zes_device_handle_t& device);

  static std::string to_string(ze_device_uuid_t val);

  static std::string to_hex_string(uint32_t val);

  static std::string get_health_state_string(zes_mem_health_t val);

  static std::string to_string(zes_pci_address_t address);

 private:
  std::unique_ptr<ThreadPool>  p_thread_pool;
  
  bool initialized;
  
  std::mutex  mutex;
    
};
