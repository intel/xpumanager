#pragma once

#include <mutex>
#include <vector>

#include "device_capability.h"
#include "init_close_interface.h"
#include "device.h"
#include "measurement_data.h"
#include "measurement_type.h"
#include "scheduler.h"
#include "standby.h"
#include "power.h"
#include "frequency.h"

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
                                               
  
  virtual std::shared_ptr<Device> getDevice(const std::string& id) = 0;
};
