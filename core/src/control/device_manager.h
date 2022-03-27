/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device_manager.h
 */

#pragma once
#include <map>
#include <mutex>
#include <vector>

#include "data_logic/data_logic_interface.h"
#include "device/memoryEcc.h"
#include "device/scheduler.h"
#include "device_manager_interface.h"

namespace xpum {

/* 
  DeviceManager class provides various interfaces for managing all devices.
*/

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

    bool setDevicePowerSustainedLimits(const std::string& id, int32_t tileId,
                                       const Power_sustained_limit_t& sustained_limit);

    bool setDevicePowerBurstLimits(const std::string& id,
                                   const Power_burst_limit_t& burst_limit);

    bool setDevicePowerPeakLimits(const std::string& id,
                                  const Power_peak_limit_t& peak_limit);

    void getDeviceFrequencyRanges(const std::string& id,
                                  std::vector<Frequency>& frequencies);

    void getFreqAvailableClocks(const std::string& id, uint32_t subdevice_id, std::vector<double>& clocks);

    bool setDeviceFrequencyRange(const std::string& id,
                                 const Frequency& freq);

    bool setDeviceStandby(const std::string& id,
                          const Standby& standby);

    bool setDeviceSchedulerTimeoutMode(const std::string& id,
                                       const SchedulerTimeoutMode& mode);

    bool setDeviceSchedulerTimesliceMode(const std::string& id,
                                         const SchedulerTimesliceMode& mode);

    bool setDeviceSchedulerExclusiveMode(const std::string& id,
                                         const SchedulerExclusiveMode& mode);

    bool resetDevice(const std::string& id, bool force);

    void getDeviceProcessState(const std::string& id, std::vector<device_process>& processes);

    void getPerformanceFactor(const std::string& id, std::vector<PerformanceFactor>& pf);

    bool setPerformanceFactor(const std::string& id, PerformanceFactor& pf);

    bool getFabricPorts(const std::string& id, std::vector<port_info>& portInfo);

    bool setFabricPorts(const std::string& id, const port_info_set& portInfoSet);

    bool getEccState(const std::string& id, MemoryEcc& ecc);
    bool setEccState(const std::string& id, ecc_state_t& newState, MemoryEcc& ecc);

    std::shared_ptr<Device> getDevice(const std::string& id);

    void discoverFabricLinks();

    std::string getDeviceIDByFabricID(uint64_t fabric_id);

   private:
    DeviceManager() = default;

    DeviceManager& operator=(const DeviceManager&) = delete;

    DeviceManager(const DeviceManager& other) = delete;

    zes_device_handle_t getDeviceHandle(const std::string& Id);

   private:
    std::shared_ptr<DataLogicInterface> p_data_logic;

    std::vector<std::shared_ptr<Device>> devices;

    std::map<uint32_t, std::string> fabric_ids;

    std::mutex mutex;
};

} // end namespace xpum
