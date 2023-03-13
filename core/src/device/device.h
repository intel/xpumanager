/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device.h
 */

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <atomic>

#include "../include/xpum_structs.h"
#include "engine_info.h"
#include "infrastructure/device_capability.h"
#include "infrastructure/exception/base_exception.h"
#include "infrastructure/measurement_type.h"
#include "infrastructure/property.h"
#include "level_zero/ze_api.h"
#include "level_zero/zes_api.h"
#include "level_zero/zet_api.h"

namespace xpum {

class FwDataMgmt;
class PscMgmt;

struct RunGSCFirmwareFlashParam;
struct GetGSCFirmwareFlashResultParam;

/*
  Device class defines various interfaces for communication with devices.
*/

typedef std::function<void(std::shared_ptr<void>, std::shared_ptr<BaseException>)> Callback_t;

enum FabricThroughputType {
    RECEIVED = 0,
    TRANSMITTED = 1,
    RECEIVED_COUNTER = 2,
    TRANSMITTED_COUNTER = 3,
    FABRIC_THROUGHPUT_TYPE_MAX = 4,
};

typedef struct FabricThroughputInfo_t {
    uint32_t attach_id;
    uint32_t remote_fabric_id;
    uint32_t remote_attach_id;
    FabricThroughputType type;
} FabricThroughputInfo;


struct pci_address_t {
    uint32_t domain;    ///< BDF domain
    uint32_t bus;       ///< BDF bus
    uint32_t device;    ///< BDF device
    uint32_t function;  ///< BDF function

    bool operator==(const pci_address_t& addr) {
        return domain == addr.domain &&
               bus == addr.bus && device == addr.device && function == addr.function;
    }
};

class Device {
   public:
    Device();

    std::string getId() noexcept;

    void getCapability(std::vector<DeviceCapability>& capabilites) noexcept;

    bool hasCapability(DeviceCapability& cap) noexcept;

    void getProperties(std::vector<Property>& properties) noexcept;

    bool getProperty(xpum_device_internal_property_name_t name, Property& ret) noexcept;

    virtual void getPower(Callback_t callback) noexcept = 0;

    virtual void getActuralRequestFrequency(Callback_t callback) noexcept = 0;

    virtual void getTemperature(Callback_t callback, zes_temp_sensors_t type) noexcept = 0;

    virtual void getMemoryUsedUtilization(Callback_t callback) noexcept = 0;

    virtual void getMemoryBandwidth(Callback_t callback) noexcept = 0;

    virtual void getMemoryReadWrite(Callback_t callback) noexcept = 0;

    virtual void getEngineUtilization(Callback_t callback) noexcept = 0;

    virtual void getGPUUtilization(Callback_t callback) noexcept = 0;

    virtual void getEngineGroupUtilization(Callback_t callback, zes_engine_group_t engine_group_type) noexcept = 0;

    virtual void getEnergy(Callback_t callback) noexcept = 0;

    virtual void getEuActiveStallIdle(Callback_t callback, MeasurementType type) noexcept = 0;

    virtual void getRasError(Callback_t callback, const zes_ras_error_cat_t& rasCat, const zes_ras_error_type_t& rasType) noexcept = 0;

    virtual void getRasErrorOnSubdevice(Callback_t callback, const zes_ras_error_cat_t& rasCat, const zes_ras_error_type_t& rasType) noexcept = 0;
    virtual void getRasErrorOnSubdevice(Callback_t callback) noexcept = 0;

    virtual void getFrequencyThrottle(Callback_t callback) noexcept = 0;

    virtual void getFrequencyThrottleReason(Callback_t callback) noexcept = 0;

    virtual void getPCIeReadThroughput(Callback_t callback) noexcept = 0;

    virtual void getPCIeWriteThroughput(Callback_t callback) noexcept = 0;

    virtual void getPCIeRead(Callback_t callback) noexcept = 0;

    virtual void getPCIeWrite(Callback_t callback) noexcept = 0;

    virtual void getFabricThroughput(Callback_t callback) noexcept = 0;

    virtual void getPerfMetrics(Callback_t callback) noexcept = 0;

    void addCapability(DeviceCapability& capability);

    void removeCapability(DeviceCapability& capability);

    void addProperty(Property prop);

    void removeProperty(xpum_device_internal_property_name_t name);

    zes_device_handle_t getDeviceHandle();

    virtual xpum_result_t runFirmwareFlash(RunGSCFirmwareFlashParam &param) noexcept; // GSC
    virtual xpum_firmware_flash_result_t getFirmwareFlashResult(GetGSCFirmwareFlashResultParam &param) noexcept;

    ze_device_handle_t getDeviceZeHandle();

    ze_driver_handle_t getDriverHandle();

    virtual bool isUpgradingFw(void) noexcept;

    virtual bool isUpgradingFwResultReady(void) noexcept = 0;

    static std::function<void(Callback_t)> getDeviceMethod(DeviceCapability& capability, Device* p_device);

    void addEngine(uint64_t engine, zes_engine_group_t type, bool on_subdevice, uint32_t subdevice_id);

    uint32_t getEngineCount() noexcept;

    uint32_t getEngineCount(int32_t subdevice_id, zes_engine_group_t type);

    uint32_t getEngineIndex(uint64_t handle);

    void setFabricID(uint32_t fabric_id);

    void addFabricPortHandle(uint32_t attach_id, uint32_t remote_fabric_id, uint32_t remote_attach_id, zes_fabric_port_handle_t handle);

    uint64_t getFabricThroughputID(uint32_t attach_id, uint32_t remote_fabric_id, uint32_t remote_attach_id, FabricThroughputType type);

    std::map<uint32_t, std::map<uint32_t, std::map<uint32_t, std::vector<zes_fabric_port_handle_t>>>> getThroughputHandles();

    bool getFabricThroughputInfo(uint64_t throughput_id, FabricThroughputInfo& info);

    uint32_t getFabricThroughputInfoCount();

    std::map<uint32_t, std::map<uint32_t, std::map<uint32_t, std::vector<FabricThroughputType>>>> getFabricThroughputIDS();

    void setPciAddress(pci_address_t address) {
        bdfAddr = address;
    }

    pci_address_t getPciAddress() {
        return bdfAddr;
    }

    void setMeiDevicePath(std::string path) {
        mei_device_path = path;
    }

    std::string getMeiDevicePath() {
        return mei_device_path;
    }

    void setFwDataMgmt(std::shared_ptr<FwDataMgmt> pFwDataMgmt) {
        this->pFwDataMgmt = pFwDataMgmt;
    }

    std::shared_ptr<FwDataMgmt> getFwDataMgmt() {
        return pFwDataMgmt;
    }

    void setPscMgmt(std::shared_ptr<PscMgmt> pPscMgmt) {
        this->pPscMgmt = pPscMgmt;
    }

    std::shared_ptr<PscMgmt> getPscMgmt() {
        return pPscMgmt;
    }

    bool try_lock() {
        if (_operation_lock.test_and_set()) {
            return false;
        }
        return true;
    }

    void unlock() {
        _operation_lock.clear();
    }

    int getDeviceModel();

    std::atomic<int> gscFwFlashPercent;

   public:
    virtual ~Device() {}

   protected:
    std::string id;

    zes_device_handle_t zes_device_handle;

    ze_device_handle_t ze_device_handle;

    ze_driver_handle_t ze_driver_handle;

    std::mutex mutex;

    std::vector<DeviceCapability> capabilities;

    std::vector<Property> properties;

    std::map<uint64_t, EngineInfo> engines;

    uint32_t fabric_id;

    std::map<uint32_t, std::map<uint32_t, std::map<uint32_t, std::vector<zes_fabric_port_handle_t>>>> connected_fabric_port_handles;

    std::map<uint32_t, std::map<uint32_t, std::map<uint32_t, std::vector<FabricThroughputType>>>> fabric_throughput_ids;

    std::map<uint64_t, FabricThroughputInfo> fabric_throughput_info;

    pci_address_t bdfAddr;

    std::string mei_device_path;

    std::shared_ptr<FwDataMgmt> pFwDataMgmt;

    std::shared_ptr<PscMgmt> pPscMgmt;

   private:
    std::atomic_flag _operation_lock = ATOMIC_FLAG_INIT;
};

} // end namespace xpum
