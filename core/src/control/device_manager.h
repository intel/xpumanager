#pragma once
#include <mutex>
#include <vector>
#include <map>

#include "data_logic_interface.h"
#include "device_manager_interface.h"
#include "scheduler.h"

class DeviceManager : public DeviceManagerInterface,
                      public std::enable_shared_from_this<DeviceManager> {
 public:
  DeviceManager(std::shared_ptr<DataLogicInterface>&);

  ~DeviceManager();

  void init() override;

  void close() override;

  void getDeviceList(std::vector<std::shared_ptr<Device>>& devices) override;

  void getDeviceList(DeviceCapability cap,
                     std::vector<std::shared_ptr<Device>>& devices) override;

  MeasurementData getRealtimeMeasurementData(MeasurementType type,
                                             std::string& device_id) override;
  
  void getDeviceSchedulers(const std::string& id, std::vector<Scheduler>& schedulers) override;

  void getDeviceStandbys(const std::string& id, std::vector<Standby>& standbys) override;

  void getDevicePowerProps(const std::string& id,
                           std::vector<Power>& powers);

  void getDevicePowerLimits(const std::string& id,
                            Power_sustained_limit_t& sustained_limit,
                            Power_burst_limit_t& burst_limit,
                            Power_peak_limit_t& peak_limit);
  
  bool setDevicePowerSustainedLimits(const std::string& id,
                                     const Power_sustained_limit_t& sustained_limit);

  bool setDevicePowerBurstLimits(const std::string& id,
                                 const Power_burst_limit_t& burst_limit);

  bool setDevicePowerPeakLimits(const std::string& id,
                                const Power_peak_limit_t& peak_limit);
  
  void getDeviceFrequencyRanges(const std::string& id,
                                std::vector<Frequency>& frequencies);
  
  bool setDeviceFrequencyRange(const std::string& id,
                               const Frequency& freq);

 private:
  DeviceManager() = default;

  DeviceManager& operator=(const DeviceManager&) = delete;

  DeviceManager(const DeviceManager& other) = delete;

  std::shared_ptr<Device> getDevice(const std::string& id);

  zes_device_handle_t getDeviceHandle(const std::string& Id);

 private:
  std::shared_ptr<DataLogicInterface> p_data_logic;

  std::vector<std::shared_ptr<Device>> devices;

  std::mutex mutex;
};
