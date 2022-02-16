#pragma once

#include <mutex>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <map>

#include "../include/xpum_structs.h"
#include "infrastructure/device_capability.h"
#include "infrastructure/measurement_type.h"
#include "infrastructure/exception/base_exception.h"
#include "infrastructure/property.h"
#include "level_zero/ze_api.h"
#include "level_zero/zes_api.h"
#include "level_zero/zet_api.h"


namespace xpum {

/*
  Device class defines various interfaces for communication with devices.
*/

typedef std::function<void(std::shared_ptr<void>, std::shared_ptr<BaseException>)> Callback_t;

class Device {
   public:
    std::string getId() noexcept;

    void getCapability(std::vector<DeviceCapability>& capabilites) noexcept;

    bool hasCapability(DeviceCapability& cap) noexcept;

    void getProperties(std::vector<Property>& properties) noexcept;

    bool getProperty(xpum_device_internal_property_name_t name, Property& ret) noexcept;

    virtual void getPower(Callback_t callback) noexcept = 0;

    virtual void getActuralFrequency(Callback_t callback) noexcept = 0;

    virtual void getRequestFrequency(Callback_t callback) noexcept = 0;

    virtual void getTemperature(Callback_t callback, zes_temp_sensors_t type) noexcept = 0;

    virtual void getMemory(Callback_t callback) noexcept = 0;

    virtual void getMemoryUtilization(Callback_t callback) noexcept = 0;

    virtual void getMemoryBandwidth(Callback_t callback) noexcept = 0;

    virtual void getMemoryRead(Callback_t callback) noexcept = 0;

    virtual void getMemoryWrite(Callback_t callback) noexcept = 0;

    virtual void getMemoryReadThroughput(Callback_t callback) noexcept = 0;

    virtual void getMemoryWriteThroughput(Callback_t callback) noexcept = 0;

    virtual void getEngineUtilization(Callback_t callback) noexcept = 0;

    virtual void getGPUUtilization(Callback_t callback) noexcept = 0;

    virtual void getEngineGroupUtilization(Callback_t callback, zes_engine_group_t engine_group_type) noexcept = 0;

    virtual void getEnergy(Callback_t callback) noexcept = 0;

    virtual void getEuActiveStallIdle(Callback_t callback, MeasurementType type) noexcept = 0;

    virtual void getRasError(Callback_t callback, const zes_ras_error_cat_t& rasCat, const zes_ras_error_type_t& rasType) noexcept = 0;

    virtual void getRasErrorOnSubdevice(Callback_t callback, const zes_ras_error_cat_t& rasCat, const zes_ras_error_type_t& rasType) noexcept = 0;

    virtual void getFrequencyThrottle(Callback_t callback) noexcept = 0;
  
    virtual void getPCIeReadThroughput(Callback_t callback) noexcept = 0;

    virtual void getPCIeWriteThroughput(Callback_t callback) noexcept = 0;
  
    virtual void getPCIeRead(Callback_t callback) noexcept = 0;

    virtual void getPCIeWrite(Callback_t callback) noexcept = 0;

    void addCapability(DeviceCapability& capability);

    void removeCapability(DeviceCapability& capability);

    void addProperty(Property prop);

    void removeProperty(xpum_device_internal_property_name_t name);

    zes_device_handle_t getDeviceHandle();

    virtual xpum_result_t runFirmwareFlash(const char* filePath, const std::string& toolPath) noexcept; //GSC
    virtual xpum_result_t runFirmwareFlash(const char* filePath) noexcept; //AMC
    virtual xpum_firmware_flash_result_t getFirmwareFlashResult(xpum_firmware_type_t type) noexcept;

    ze_device_handle_t getDeviceZeHandle();

    ze_driver_handle_t getDriverHandle();

    virtual bool isUpgradingFw(void) noexcept;

    static std::function<void(Callback_t)> getDeviceMethod(DeviceCapability& capability, Device* p_device);

    void addEngine(uint64_t engine);

    uint32_t getEngineCount() noexcept;

    uint32_t getEngineID(uint64_t handle);

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

    std::map<uint64_t,uint32_t> engines;
};

} // end namespace xpum
