/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file gpu_device.h
 */

#pragma once

#include <future>
#include <mutex>

#include "device/device.h"
#include "stdio.h"

namespace xpum {
/*
  GPUDevice class defines various interfaces for communication with GPU devices.
*/

class GPUDevice : public Device {
   public:
    GPUDevice();
    GPUDevice(const std::string& id, const zes_device_handle_t& zes_device, std::vector<DeviceCapability>& capabilities);
    GPUDevice(const std::string& id, const zes_device_handle_t& zes_device, const ze_device_handle_t& ze_device, const ze_driver_handle_t& driver, std::vector<DeviceCapability>& capabilities);

    virtual ~GPUDevice();

   public:
    void getPower(Callback_t callback) noexcept override;
    void getActuralRequestFrequency(Callback_t callback) noexcept override;
    void getTemperature(Callback_t callback, zes_temp_sensors_t type) noexcept override;
    void getMemoryUsedUtilization(Callback_t callback) noexcept override;
    void getMemoryBandwidth(Callback_t callback) noexcept override;
    void getMemoryReadWrite(Callback_t callback) noexcept override;
    void getGPUUtilization(Callback_t callback) noexcept override;
    void getEngineUtilization(Callback_t callback) noexcept override;
    void getEngineGroupUtilization(Callback_t callback, zes_engine_group_t engine_group_type) noexcept override;
    void getEnergy(Callback_t callback) noexcept override;
    void getEuActiveStallIdle(Callback_t callback, MeasurementType type) noexcept override;
    void getRasError(Callback_t callback, const zes_ras_error_cat_t& rasCat, const zes_ras_error_type_t& rasType) noexcept override;
    void getRasErrorOnSubdevice(Callback_t callback, const zes_ras_error_cat_t& rasCat, const zes_ras_error_type_t& rasType) noexcept override;
    void getRasErrorOnSubdevice(Callback_t callback) noexcept override;
    void getFrequencyThrottle(Callback_t callback) noexcept override;
    void getFrequencyThrottleReason(Callback_t callback) noexcept override;
    void getPCIeReadThroughput(Callback_t callback) noexcept override;
    void getPCIeWriteThroughput(Callback_t callback) noexcept override;
    void getPCIeRead(Callback_t callback) noexcept override;
    void getPCIeWrite(Callback_t callback) noexcept override;
    void getFabricThroughput(Callback_t callback) noexcept override;
    void getPerfMetrics(Callback_t callback) noexcept override;

    virtual xpum_result_t runFirmwareFlash(RunGSCFirmwareFlashParam &param) noexcept override; // GSC
    virtual xpum_firmware_flash_result_t getFirmwareFlashResult(GetGSCFirmwareFlashResultParam &param) noexcept override;

    virtual bool isUpgradingFw(void) noexcept override;
    virtual bool isUpgradingFwResultReady(void) noexcept override;

   private:
    void dumpFirmwareFlashLog() noexcept;

   private:
    //FILE* commandExec;
    std::future<xpum_firmware_flash_result_t>  taskGSC;
    std::mutex mtx;
    std::string log;

    static const unsigned int BUFFERSIZE = 4 * 1024;
    static const std::string logFilePath;

    std::string flashFwErrMsg;
};

} // end namespace xpum
