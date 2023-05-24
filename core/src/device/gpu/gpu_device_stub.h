/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file gpu_device_stub.h
 */

#pragma once

#include <dirent.h>
#include <dlfcn.h>

#include <string>

#include "device/device.h"
#include "device/frequency.h"
#include "device/memoryEcc.h"
#include "device/pcie_manager.h"
#include "device/performancefactor.h"
#include "device/power.h"
#include "device/scheduler.h"
#include "device/standby.h"
#include "device/skuType.h"
#include "health/health_data_type.h"
#include "infrastructure/const.h"
#include "infrastructure/device_process.h"
#include "infrastructure/device_util_by_proc.h"
#include "infrastructure/engine_measurement_data.h"
#include "infrastructure/fabric_measurement_data.h"
#include "infrastructure/measurement_data.h"
#include "infrastructure/perf_measurement_data.h"
#include "level_zero/ze_api.h"
#include "level_zero/zes_api.h"
#include "level_zero/zet_api.h"
#include "topology/xe_link.h"

namespace xpum {

struct DeviceMetricGroups_t {
  std::string group_name;
  uint32_t domain;
  uint32_t metric_count;
  zet_metric_group_handle_t metric_group;
  zet_metric_streamer_handle_t streamer;
  std::map<std::string, std::shared_ptr<PerfMetricData_t>> target_metrics;
};

typedef ze_result_t (*pFnzexMemoryGetBandwidth)(zes_mem_handle_t,
                uint64_t *, uint64_t *, uint64_t *, uint64_t);
/*
  GPUDeviceStub class provides various capabilities to communicate with GPU devices.
*/

class GPUDeviceStub {
   public:
    static GPUDeviceStub& instance();

    static PCIeManager pcie_manager;

   public:
    void discoverDevices(Callback_t callback);

    void getPower(const zes_device_handle_t& device, Callback_t callback) noexcept;

    void getActuralRequestFrequency(const zes_device_handle_t& device, Callback_t callback) noexcept;

    void getTemperature(const zes_device_handle_t& device, Callback_t callback, zes_temp_sensors_t type) noexcept;

    void getMemoryUsedUtilization(const zes_device_handle_t& device, Callback_t callback) noexcept;

    void getMemoryBandwidth(const zes_device_handle_t& device, Callback_t callback) noexcept;

    void getMemoryReadWrite(const zes_device_handle_t& device, Callback_t callback) noexcept;

    void getGPUUtilization(const zes_device_handle_t& device, Callback_t callback) noexcept;

    void getEngineUtilization(const zes_device_handle_t& device, Callback_t callback) noexcept;

    void getEngineGroupUtilization(const zes_device_handle_t& device, Callback_t callback, zes_engine_group_t engine_group_type) noexcept;

    void getEnergy(const zes_device_handle_t& device, Callback_t callback) noexcept;

    void getEuActiveStallIdle(const ze_device_handle_t& device, const ze_driver_handle_t& driver, MeasurementType type, Callback_t callback) noexcept;

    void getRasError(const zes_device_handle_t& device, Callback_t callback, const zes_ras_error_cat_t& rasCat, const zes_ras_error_type_t& rasType) noexcept;

    void getRasErrorOnSubdevice(const zes_device_handle_t& device, Callback_t callback, const zes_ras_error_cat_t& rasCat, const zes_ras_error_type_t& rasType) noexcept;
    void getRasErrorOnSubdevice(const zes_device_handle_t& device, Callback_t callback) noexcept;

    void getRasError(const zes_device_handle_t& device, uint64_t errorCategory[XPUM_RAS_ERROR_MAX]) noexcept;

    void getFrequencyThrottle(const zes_device_handle_t& device, Callback_t callback) noexcept;

    void getFrequencyThrottleReason(const zes_device_handle_t& device, Callback_t callback) noexcept;

    void getPCIeReadThroughput(const zes_device_handle_t& device, Callback_t callback) noexcept;

    void getPCIeWriteThroughput(const zes_device_handle_t& device, Callback_t callback) noexcept;

    void getPCIeRead(const zes_device_handle_t& device, Callback_t callback) noexcept;

    void getPCIeWrite(const zes_device_handle_t& device, Callback_t callback) noexcept;

    static void getSchedulers(const zes_device_handle_t& device, std::vector<Scheduler>& schedulers);

    static void getStandbys(const zes_device_handle_t& device, std::vector<Standby>& standbys);

    static void getPowerProps(const zes_device_handle_t& device, std::vector<Power>& powers);

    static void getPerformanceFactor(const zes_device_handle_t& device, std::vector<PerformanceFactor>& pf);
    static bool setPerformanceFactor(const zes_device_handle_t& device, PerformanceFactor& pf);
    bool getEccState(const zes_device_handle_t& device, MemoryEcc& ecc);
    bool setEccState(const zes_device_handle_t& device, ecc_state_t& newState, MemoryEcc& ecc);

    void getFabricThroughput(const zes_device_handle_t& device, Callback_t callback) noexcept;

    void getPerfMetrics(zes_device_handle_t& device, ze_driver_handle_t& driver, Callback_t callback) noexcept;

    static void getPowerLimits(const zes_device_handle_t& device,
                               Power_sustained_limit_t& sustained_limit,
                               Power_burst_limit_t& burst_limit,
                               Power_peak_limit_t& peak_limit);
    static void getAllPowerLimits(const zes_device_handle_t& device,
                                  std::vector<uint32_t>& tileIds,
                                  std::vector<Power_sustained_limit_t>& sustained_limits,
                                  std::vector<Power_burst_limit_t>& burst_limits,
                                  std::vector<Power_peak_limit_t>& peak_limit);

    static bool setPowerSustainedLimits(const zes_device_handle_t& device, int32_t tileId,
                                        const Power_sustained_limit_t& sustained_limit);

    static bool setPowerBurstLimits(const zes_device_handle_t& device,
                                    const Power_burst_limit_t& burst_limit);

    static bool setPowerPeakLimits(const zes_device_handle_t& device,
                                   const Power_peak_limit_t& peak_limit);

    static void getFrequencyRanges(const zes_device_handle_t& device,
                                   std::vector<Frequency>& frequencies);

    static bool setFrequencyRangeForAll(const zes_device_handle_t& device,
                                  const Frequency& freq);

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

    static bool setSchedulerDebugMode(const zes_device_handle_t& device,
                                          const SchedulerDebugMode& mode);
    static bool getFrequencyState(const zes_device_handle_t& device, std::string& freq_throttle_message);

    static void getHealthStatus(const zes_device_handle_t& device, xpum_health_type_t type, xpum_health_data_t* data,
                                int core_thermal_threshold, int memory_thermal_threshold, int power_threshold, bool global_default_limit);

    static bool resetDevice(const zes_device_handle_t& device, ze_bool_t force);

    void getDeviceProcessState(const zes_device_handle_t& device, std::vector<device_process>& processes);

    static bool getDeviceUtilByProc(
        const std::vector<zes_device_handle_t>& devices, 
        const std::vector<std::string>& device_ids, uint32_t utilInterval,
        std::vector<std::vector<device_util_by_proc>>& utils);

    static void getFreqAvailableClocks(const zes_device_handle_t& device, uint32_t subdevice_id, std::vector<double>& clocks);

    static std::shared_ptr<MeasurementData> toGetPower(const zes_device_handle_t& device);

    static std::shared_ptr<MeasurementData> toGetRasError(const zes_device_handle_t& device, const zes_ras_error_cat_t& rasCat, const zes_ras_error_type_t& rasType);

    static bool getFabricPorts(const zes_device_handle_t& device, std::vector<port_info>& portInfo);

    static bool setFabricPorts(const zes_device_handle_t& device, const port_info_set& portInfoSet);

    static std::shared_ptr<FabricMeasurementData> toGetFabricThroughput(const zes_device_handle_t& device);

    static std::shared_ptr<MeasurementData> loadPVCIdlePowers(std::string bdf = "", bool fresh = true, int index = 0);

    static std::string getPciSlotByPath(std::vector<std::string> pciPath); 

    static bool getZexGetMemoryBandwidth(pFnzexMemoryGetBandwidth *pFunc);

private: 
    GPUDeviceStub(); 
    ~GPUDeviceStub();

    GPUDeviceStub& operator=(const GPUDeviceStub&) = delete;

    GPUDeviceStub(const GPUDeviceStub& other) = delete;

    void init();

    static std::shared_ptr<std::vector<std::shared_ptr<Device>>> toDiscover();

    static std::shared_ptr<MeasurementData> toGetActuralRequestFrequency(const zes_device_handle_t& device);

    static std::shared_ptr<MeasurementData> toGetTemperature(const zes_device_handle_t& device, zes_temp_sensors_t type);

    static std::shared_ptr<MeasurementData> toGetMemoryUsedUtilization(const zes_device_handle_t& device);

    static std::shared_ptr<MeasurementData> toGetMemoryBandwidth(const zes_device_handle_t& device);

    static std::shared_ptr<MeasurementData> toGetMemoryReadWrite(const zes_device_handle_t& device);

    static std::shared_ptr<MeasurementData> toGetGPUUtilization(const zes_device_handle_t& device);

    static std::shared_ptr<EngineCollectionMeasurementData> toGetEngineUtilization(const zes_device_handle_t& device);

    static std::shared_ptr<MeasurementData> toGetEngineGroupUtilization(const zes_device_handle_t& device, zes_engine_group_t group_type);

    static std::shared_ptr<MeasurementData> toGetEnergy(const zes_device_handle_t& device);

    static std::shared_ptr<MeasurementData> toGetEuActiveStallIdle(const ze_device_handle_t& device, const ze_driver_handle_t& driver, MeasurementType type);

    static void toGetEuActiveStallIdleCore(const ze_device_handle_t& device, uint32_t subdeviceId, const ze_driver_handle_t& driver, MeasurementType type, std::shared_ptr<MeasurementData>& data);

    static std::shared_ptr<PerfMeasurementData> toGetPerfMetrics(ze_device_handle_t& device, ze_driver_handle_t& driver);

    //static std::shared_ptr<MeasurementData> toGetRasError(const zes_device_handle_t& device, const zes_ras_error_cat_t& rasCat, const zes_ras_error_type_t& rasType);

    static std::shared_ptr<MeasurementData> toGetRasErrorOnSubdeviceOld(const zes_device_handle_t& device, const zes_ras_error_cat_t& rasCat, const zes_ras_error_type_t& rasType);
    static std::shared_ptr<MeasurementData> toGetRasErrorOnSubdevice(const zes_device_handle_t& device);

    static std::shared_ptr<MeasurementData> toGetFrequencyThrottle(const zes_device_handle_t& device);

    static std::shared_ptr<MeasurementData> toGetFrequencyThrottleReason(const zes_device_handle_t& device);

    static std::shared_ptr<MeasurementData> toGetPCIeReadThroughput(const zes_device_handle_t& device);

    static std::shared_ptr<MeasurementData> toGetPCIeWriteThroughput(const zes_device_handle_t& device);

    static std::shared_ptr<MeasurementData> toGetPCIeRead(const zes_device_handle_t& device);

    static std::shared_ptr<MeasurementData> toGetPCIeWrite(const zes_device_handle_t& device);

    static std::string to_string(ze_device_uuid_t val);

    static std::string to_hex_string(uint32_t val);

    static std::string get_health_state_string(zes_mem_health_t val);

    static std::string get_freq_throttle_string(zes_freq_throttle_reason_flags_t flags);

    static std::string to_string(zes_pci_address_t address);

    static std::string to_regex_string(zes_pci_address_t address);

    static int get_register_value_from_sys(const zes_device_handle_t& device, uint64_t offset);

    static void addEuActiveStallIdleCapabilities(zes_device_handle_t device, const zes_device_properties_t& props, ze_driver_handle_t driver, std::vector<DeviceCapability>& capabilities);

    static void addCapabilities(zes_device_handle_t device, const zes_device_properties_t& props, std::vector<DeviceCapability>& capabilities);

    static void addEngineCapabilities(zes_device_handle_t device, const zes_device_properties_t& props, std::vector<DeviceCapability>& capabilities);

    static void checkInitDependency();

    static bool isDevEntry(const std::string& entryName);

    static std::string buildErrors(const std::map<std::string, ze_result_t>& exception_msgs, const char* func, uint32_t line);
    static std::string getProcessName(uint32_t processId);

    static void logSupportedMetrics(zes_device_handle_t device, const zes_device_properties_t& props, const std::vector<DeviceCapability>& capabilities);
    
    static std::string getDRMDevice(const zes_pci_properties_t& pci_props);

    static std::shared_ptr<std::vector<std::shared_ptr<DeviceMetricGroups_t>>> getDevicePerfMetricGroups(ze_device_handle_t& device, 
                                                                                                         ze_driver_handle_t& driver);

    static void readPerfMetricsData(std::shared_ptr<std::map<uint32_t, std::shared_ptr<DeviceMetricGroups_t>>>& p_groups,
                                    std::shared_ptr<PerfMetricDeviceData_t> &p_metric_device_data);
    
    static void openDevicePerfMetricStream(ze_device_handle_t& device, ze_driver_handle_t& driver, 
                                           std::shared_ptr<std::map<uint32_t, std::shared_ptr<DeviceMetricGroups_t>>>& p_target_groups,
                                           std::map<ze_device_handle_t, ze_context_handle_t>& device_contexts); 

    static std::string getPciSlot(zes_pci_address_t address);
    static std::string getOAMSocketId(zes_pci_address_t address);

   private:
    bool initialized;

    std::mutex mutex;

    static bool daemonless;

    static std::mutex metric_streamer_mutex;

    static std::map<ze_device_handle_t, zet_metric_group_handle_t> target_metric_groups;

    static std::map<ze_device_handle_t, ze_context_handle_t> target_metric_contexts;

    static std::map<ze_device_handle_t, std::shared_ptr<std::vector<std::shared_ptr<DeviceMetricGroups_t>>>> device_perf_groups;

    static std::mutex pvc_idle_power_mutex;
    static std::map<std::string, std::shared_ptr<MeasurementData>> pvc_idle_powers; // key: bdf value: idle_power
    static std::set<std::string> pvc_gpu_bdfs;
    static bool has_pvc_idle_powers;

    static std::mutex fabric_mutex;
};

} // end namespace xpum
