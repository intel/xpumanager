#pragma once

#include <string>
#include <dlfcn.h>
#include <dirent.h>
#include "infrastructure/const.h"
#include "device/device.h"
#include "infrastructure/measurement_data.h"
#include "infrastructure/thread_pool.h"
#include "level_zero/ze_api.h"
#include "level_zero/zes_api.h"
#include "level_zero/zet_api.h"
#include "device/scheduler.h"
#include "device/standby.h"
#include "device/power.h"
#include "device/frequency.h"
#include "health/health_data_type.h"
#include "topology/topology.h"

class GPUDeviceStub {
 public:
  static GPUDeviceStub& instance();

 public:  
  void discoverDevices(Callback_t callback);
  
  void getPower(const zes_device_handle_t& device, Callback_t callback) noexcept;

  void getActuralFrequency(const zes_device_handle_t& device, Callback_t callback) noexcept;

  void getTemperature(const zes_device_handle_t& device, Callback_t callback) noexcept;

  void getMemory(const zes_device_handle_t& device, Callback_t callback) noexcept;

  void getMemoryUtilization(const zes_device_handle_t& device, Callback_t callback) noexcept;

  void getMemoryBandwidth(const zes_device_handle_t& device, Callback_t callback) noexcept;

  void getMemoryRead(const zes_device_handle_t& device, Callback_t callback) noexcept;

  void getMemoryWrite(const zes_device_handle_t& device, Callback_t callback) noexcept;

  void getEngineUtilization(const zes_device_handle_t& device, Callback_t callback) noexcept;

  void getEngineGroupUtilization(const zes_device_handle_t& device, Callback_t callback, zes_engine_group_t engine_group_type) noexcept;

  void getEnergy(const zes_device_handle_t& device, Callback_t callback) noexcept;

  void getOccupationEfficiency(const ze_device_handle_t& device, const ze_driver_handle_t& driver, MeasurementType type, Callback_t callback) noexcept;

  void getRasError(const zes_device_handle_t& device, Callback_t callback,const zes_ras_error_cat_t &rasCat, const zes_ras_error_type_t &rasType) noexcept;

  void getRasError(const zes_device_handle_t &device, uint64_t errorCategory[XPUM_RAS_ERROR_MAX]) noexcept;

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

  static void getHealthStatus(const zes_device_handle_t& device, xpum_health_type_t type, xpum_health_data_t *data, 
                              int thermal_threshold, int power_threshold, bool global_default_limit);
  
  static bool resetDevice(const zes_device_handle_t& device, ze_bool_t force);
  
  static void getFreqAvailableClocks(const zes_device_handle_t& device, uint32_t subdevice_id, std::vector<double>& clocks);

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

  static std::shared_ptr<MeasurementData> toGetMemoryUtilization(const zes_device_handle_t& device);

  static std::shared_ptr<MeasurementData> toGetMemoryBandwidth(const zes_device_handle_t& device);

  static std::shared_ptr<MeasurementData> toGetMemoryRead(const zes_device_handle_t& device);

  static std::shared_ptr<MeasurementData> toGetMemoryWrite(const zes_device_handle_t& device);

  static std::shared_ptr<MeasurementData> toGetEngineUtilization(const zes_device_handle_t& device);

  static std::shared_ptr<MeasurementData> toGetEngineGroupUtilization(const zes_device_handle_t& device, zes_engine_group_t group_type);

  static std::shared_ptr<MeasurementData> toGetEnergy(const zes_device_handle_t& device);

  static std::shared_ptr<MeasurementData> toGetOccupationEfficiency(const ze_device_handle_t &device, const ze_driver_handle_t& driver, MeasurementType type);

  static void toGetOccupationEfficiencyCore(const ze_device_handle_t &device, int subdeviceId, const ze_driver_handle_t& driver, MeasurementType type, std::shared_ptr<MeasurementData>& data);

  static std::shared_ptr<MeasurementData> toGetRasError(const zes_device_handle_t &device, const zes_ras_error_cat_t &rasCat, const zes_ras_error_type_t &rasType);

  static std::string to_string(ze_device_uuid_t val);

  static std::string to_hex_string(uint32_t val);

  static std::string get_health_state_string(zes_mem_health_t val);

  static std::string to_string(zes_pci_address_t address);

  static std::string to_regex_string(zes_pci_address_t address);

  static void addEgnineCapabilities(zes_device_handle_t device, std::vector<DeviceCapability>& capabilities);

  static void checkInitDependency();

  static bool isDevEntry(const std::string &entryName);
  
 private:
  std::unique_ptr<ThreadPool>  p_thread_pool;
  
  bool initialized;
  
  std::mutex  mutex;

  static std::mutex metric_streamer_mutex;
  
  static std::map<ze_device_handle_t, zet_metric_group_handle_t> target_metric_groups;
  
  static std::map<ze_device_handle_t, zet_metric_streamer_handle_t> target_metric_streamers;
};
