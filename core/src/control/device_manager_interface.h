#pragma once

#include <mutex>
#include <vector>

#include "infrastructure/device_capability.h"
#include "infrastructure/init_close_interface.h"
#include "device/device.h"
#include "infrastructure/measurement_data.h"
#include "infrastructure/measurement_type.h"
#include "device/scheduler.h"
#include "device/standby.h"
#include "device/power.h"
#include "device/frequency.h"

/* 
  DeviceManagerInterface class defines various interfaces for managing devices.
*/

class DeviceManagerInterface : public InitCloseInterface {
 public:
  virtual ~DeviceManagerInterface() {}

  virtual void getDeviceList(std::vector<std::shared_ptr<Device>>& devices) = 0;

  virtual void getDeviceList(DeviceCapability cap,
                             std::vector<std::shared_ptr<Device>>& devices) = 0;

  virtual MeasurementData getRealtimeMeasurementData(
      MeasurementType type, std::string& device_id) = 0;
    
  virtual void getDeviceSchedulers(const std::string& id, 
                                   std::vector<Scheduler>& schedulers) = 0;

  virtual void getDeviceStandbys(const std::string& id, 
                                 std::vector<Standby>& standbys) = 0;
  
  virtual void getDevicePowerProps(const std::string& id,
                                   std::vector<Power>& powers) = 0;

  virtual void getDevicePowerLimits(const std::string& id,
                                    Power_sustained_limit_t& sustained_limit,
                                    Power_burst_limit_t& burst_limit,
                                    Power_peak_limit_t& peak_limit) = 0;

  virtual bool setDevicePowerSustainedLimits(const std::string& id,
                                             const Power_sustained_limit_t& sustained_limit) = 0;

  virtual bool setDevicePowerBurstLimits(const std::string& id,
                                         const Power_burst_limit_t& burst_limit) = 0;

  virtual bool setDevicePowerPeakLimits(const std::string& id,
                                        const Power_peak_limit_t& peak_limit) = 0;

  virtual void getDeviceFrequencyRanges(const std::string& id,
                                        std::vector<Frequency>& frequencies) = 0;

  virtual bool setDeviceFrequencyRange(const std::string& id,
                                       const Frequency& freq) = 0;

  virtual bool setDeviceStandby(const std::string& id, 
                                const Standby& standby) = 0;

  virtual bool setDeviceSchedulerTimeoutMode(const std::string& id, 
                                             const SchedulerTimeoutMode& mode) = 0;

  virtual bool setDeviceSchedulerTimesliceMode(const std::string& id, 
                                               const SchedulerTimesliceMode& mode) = 0;

  virtual bool setDeviceSchedulerExclusiveMode(const std::string& id,
                                               const SchedulerExclusiveMode& mode) = 0;

  virtual bool resetDevice(const std::string& id, bool force) = 0;

  virtual void getFreqAvailableClocks(const std::string& id, uint32_t subdevice_id, std::vector<double>& clocks) = 0;
                                               
  
  virtual std::shared_ptr<Device> getDevice(const std::string& id) = 0;
};
