/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device_manager_interface.h
 */

#pragma once

#include <mutex>
#include <vector>

#include "device/device.h"
#include "device/frequency.h"
#include "device/memoryEcc.h"
#include "device/performancefactor.h"
#include "device/power.h"
#include "device/scheduler.h"
#include "device/standby.h"
#include "infrastructure/device_capability.h"
#include "infrastructure/device_process.h"
#include "infrastructure/device_util_by_proc.h"
#include "infrastructure/init_close_interface.h"
#include "infrastructure/measurement_data.h"
#include "infrastructure/measurement_type.h"
#include "topology/xe_link.h"

namespace xpum {

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

    virtual bool setDevicePowerSustainedLimits(const std::string& id, int32_t tileId,
                                               const Power_sustained_limit_t& sustained_limit) = 0;

    virtual bool setDevicePowerBurstLimits(const std::string& id,
                                           const Power_burst_limit_t& burst_limit) = 0;

    virtual bool setDevicePowerPeakLimits(const std::string& id,
                                          const Power_peak_limit_t& peak_limit) = 0;

    virtual void getDeviceFrequencyRanges(const std::string& id,
                                          std::vector<Frequency>& frequencies) = 0;

    virtual bool setDeviceFrequencyRange(const std::string& id,
                                         const Frequency& freq) = 0;

    virtual bool setDeviceFrequencyRangeForAll(const std::string& id,
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

    virtual void getDeviceProcessState(const std::string& id, std::vector<device_process>& processes) = 0;

    virtual void getDeviceUtilByProcess(const std::string& id, 
      uint32_t utilInterval, 
      std::vector<std::vector<device_util_by_proc>>& utils) = 0;

    virtual void getPerformanceFactor(const std::string& id, std::vector<PerformanceFactor>& pf) = 0;

    virtual bool setPerformanceFactor(const std::string& id, PerformanceFactor& pf) = 0;

    virtual bool getFabricPorts(const std::string& id, std::vector<port_info>& portInfo) = 0;

    virtual bool setFabricPorts(const std::string& id, const port_info_set& portInfoSet) = 0;

    virtual bool getEccState(const std::string& id, MemoryEcc& ecc) = 0;

    virtual bool setEccState(const std::string& id, ecc_state_t& newState, MemoryEcc& ecc) = 0;

    virtual std::shared_ptr<Device> getDevice(const std::string& id) = 0;

    virtual void discoverFabricLinks() = 0;

    virtual std::string getDeviceIDByFabricID(uint64_t fabric_id) = 0;

    virtual bool tryLockDevices(const std::vector<std::string>& deviceList) = 0;

    virtual bool tryLockDevices(std::vector<std::shared_ptr<Device>>& deviceList) = 0;

    virtual void unlockDevices(const std::vector<std::string>& deviceList) = 0;

    virtual void unlockDevices(std::vector<std::shared_ptr<Device>>& deviceList) = 0;
};

} // end namespace xpum
