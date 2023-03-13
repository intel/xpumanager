/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file gpu_device.cpp
 */

#include "gpu_device.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <igsc_lib.h>

#include "core/core.h"
#include "device/gpu/gpu_device_stub.h"
#include "group/group_manager.h"
#include "infrastructure/device_property.h"
#include "infrastructure/logger.h"
#include "stdio.h"
#include "unistd.h"
#include "firmware/igsc_err_msg.h"

namespace xpum {
using namespace std::chrono_literals;

GPUDevice::GPUDevice() {
}

GPUDevice::GPUDevice(const std::string& id,
                     const zes_device_handle_t& zes_device,
                     std::vector<DeviceCapability>& capabilities) {
    this->id = id;
    this->zes_device_handle = zes_device;
    for (DeviceCapability& cap : capabilities) {
        this->capabilities.push_back(cap);
    }
}

GPUDevice::GPUDevice(const std::string& id,
                     const zes_device_handle_t& zes_device,
                     const ze_device_handle_t& ze_device,
                     const zes_driver_handle_t& ze_driver,
                     std::vector<DeviceCapability>& capabilities) {
    this->id = id;
    this->zes_device_handle = zes_device;
    this->ze_device_handle = ze_device;
    this->ze_driver_handle = ze_driver;
    for (DeviceCapability& cap : capabilities) {
        this->capabilities.push_back(cap);
    }
}

GPUDevice::~GPUDevice() {
}

void GPUDevice::getPower(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getPower(zes_device_handle,
                                       [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                           callback(ret, e);
                                       });
}

void GPUDevice::getActuralRequestFrequency(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getActuralRequestFrequency(zes_device_handle,
                                                  [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                                      callback(ret, e);
                                                  });
}

void GPUDevice::getTemperature(Callback_t callback, zes_temp_sensors_t type) noexcept {
    GPUDeviceStub::instance().getTemperature(
        zes_device_handle,
        [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
            callback(ret, e);
        },
        type);
}

void GPUDevice::getMemoryUsedUtilization(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getMemoryUsedUtilization(zes_device_handle,
                                        [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                            callback(ret, e);
                                        });
}

void GPUDevice::getMemoryBandwidth(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getMemoryBandwidth(zes_device_handle,
                                                   [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                                       callback(ret, e);
                                                   });
}

void GPUDevice::getMemoryReadWrite(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getMemoryReadWrite(zes_device_handle,
                                                   [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                                       callback(ret, e);
                                                   });
}

void GPUDevice::getEnergy(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getEnergy(zes_device_handle,
                                        [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                            callback(ret, e);
                                        });
}

void GPUDevice::getEuActiveStallIdle(Callback_t callback, MeasurementType type) noexcept {
    GPUDeviceStub::instance().getEuActiveStallIdle(ze_device_handle, ze_driver_handle, type,
                                                   [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                                       callback(ret, e);
                                                   });
}

void GPUDevice::getRasError(Callback_t callback, const zes_ras_error_cat_t& rasCat, const zes_ras_error_type_t& rasType) noexcept {
    GPUDeviceStub::instance().getRasError(
        zes_device_handle,
        [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
            callback(ret, e);
        },
        rasCat, rasType);
}

void GPUDevice::getRasErrorOnSubdevice(Callback_t callback, const zes_ras_error_cat_t& rasCat, const zes_ras_error_type_t& rasType) noexcept {
    GPUDeviceStub::instance().getRasErrorOnSubdevice(
        zes_device_handle,
        [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
            callback(ret, e);
        },
        rasCat, rasType);
}

void GPUDevice::getRasErrorOnSubdevice(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getRasErrorOnSubdevice(
        zes_device_handle,
        [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
            callback(ret, e);
        });
}

void GPUDevice::getGPUUtilization(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getGPUUtilization(zes_device_handle,
                                                [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                                    callback(ret, e);
                                                });
}

void GPUDevice::getEngineUtilization(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getEngineUtilization(zes_device_handle,
                                                   [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                                       callback(ret, e);
                                                   });
}

void GPUDevice::getEngineGroupUtilization(Callback_t callback, zes_engine_group_t engine_group_type) noexcept {
    GPUDeviceStub::instance().getEngineGroupUtilization(
        zes_device_handle,
        [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
            callback(ret, e);
        },
        engine_group_type);
}

void GPUDevice::getFrequencyThrottle(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getFrequencyThrottle(zes_device_handle,
                                                   [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                                       callback(ret, e);
                                                   });
}

void GPUDevice::getFrequencyThrottleReason(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getFrequencyThrottleReason(
        zes_device_handle,
        [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
            callback(ret, e);
        });
}

static void progress_percentage_func(uint32_t done, uint32_t total, void* ctx) {
    uint32_t percent = (done * 100) / total;

    // store percent 
    GPUDevice* pDevice = (GPUDevice*) ctx;
    pDevice->gscFwFlashPercent.store(percent);
}

static std::string print_fw_version(const struct igsc_fw_version* fw_version) {
    std::stringstream ss;
    ss << fw_version->project[0];
    ss << fw_version->project[1];
    ss << fw_version->project[2];
    ss << fw_version->project[3];
    ss << "_";
    ss << fw_version->hotfix;
    ss << ".";
    ss << fw_version->build;
    return ss.str();
}


xpum_result_t GPUDevice::runFirmwareFlash(RunGSCFirmwareFlashParam &param) noexcept {
    auto& img = param.img;
    auto& force = param.force;
    std::lock_guard<std::mutex> lck(mtx);
    if (taskGSC.valid()) {
        // task already running
        // param.errMsg = "Task already running";
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
    } else {
        gscFwFlashPercent.store(0);
        flashFwErrMsg.clear();
        taskGSC = std::async(std::launch::async, [this, img, force] {
            std::string meiPath = getMeiDevicePath();

            if(meiPath.empty()){
                flashFwErrMsg = "Can not find MEI device path";
                unlock();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }

            XPUM_LOG_INFO("Start update GSC fw on device {}", meiPath);

            // struct img *img = NULL;
            struct igsc_device_handle handle;
            struct igsc_fw_version device_fw_version;
            // struct igsc_fw_version image_fw_version;
            igsc_progress_func_t progress_func = progress_percentage_func;
            int ret;
            // uint8_t cmp;
            struct igsc_fw_update_flags flags = {0};
            flags.force_update = force;

            memset(&handle, 0, sizeof(handle));

            ret = igsc_device_init_by_device(&handle, meiPath.c_str());
            if (ret)
            {
                flashFwErrMsg = "Cannot initialize device: " + meiPath + ", error code: " + std::to_string(ret) + " error message: " + transIgscErrCodeToMsg(ret);
                XPUM_LOG_ERROR("Cannot initialize device: {}, error code: {}, error message: {}", meiPath, ret, transIgscErrCodeToMsg(ret));
                (void)igsc_device_close(&handle);
                unlock();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }


            ret = igsc_device_fw_update_ex(&handle, (const uint8_t*)img.data(), img.size(),
                                           progress_func, this, flags);

            if (ret){
                flashFwErrMsg = "Update process failed, error code: " + std::to_string(ret) + " error message: " + transIgscErrCodeToMsg(ret);
                XPUM_LOG_ERROR("Update process failed, error code: {}, error message: {}", ret, transIgscErrCodeToMsg(ret));
                (void)igsc_device_close(&handle);
                unlock();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }

            // get new fw version
            ret = igsc_device_fw_version(&handle, &device_fw_version);
            if (ret != IGSC_SUCCESS)
            {
                XPUM_LOG_ERROR("Cannot retrieve firmware version from device: {}", meiPath);
            } else{
                std::string version = print_fw_version(&device_fw_version);
                addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_GFX_FIRMWARE_VERSION, version));

                XPUM_LOG_INFO("Device {} GSC fw flashed successfully to {}", meiPath, version);
            }

            (void)igsc_device_close(&handle);
            unlock();
            return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
        });

        return xpum_result_t::XPUM_OK;
    }
}


const std::string GPUDevice::logFilePath = "/tmp/gfx";

void GPUDevice::dumpFirmwareFlashLog() noexcept {
    std::ofstream logFile(logFilePath, std::ios_base::out | std::ios_base::trunc);
    if (logFile.is_open()) {
        logFile << log;
    }
    logFile.close();
}

xpum_firmware_flash_result_t GPUDevice::getFirmwareFlashResult(GetGSCFirmwareFlashResultParam &param) noexcept {
    std::future<xpum_firmware_flash_result_t>* task;
    task = &taskGSC;
    param.errMsg = flashFwErrMsg;

    if (task->valid()) {
        auto status = task->wait_for(0ms);
        if (status == std::future_status::ready) {
            std::lock_guard<std::mutex> lck(mtx);
            return task->get();
        } else {
            return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ONGOING;
        }
    } else {
        return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
    }
}

bool GPUDevice::isUpgradingFw(void) noexcept {
    return taskGSC.valid();
}

bool GPUDevice::isUpgradingFwResultReady(void) noexcept {
    if (!taskGSC.valid()) {
        return true;
    }
    auto status = taskGSC.wait_for(0ms);
    return status == std::future_status::ready;
}

void GPUDevice::getPCIeReadThroughput(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getPCIeReadThroughput(zes_device_handle,
                                                    [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                                        callback(ret, e);
                                                    });
}

void GPUDevice::getPCIeWriteThroughput(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getPCIeWriteThroughput(zes_device_handle,
                                                     [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                                         callback(ret, e);
                                                     });
}

void GPUDevice::getPCIeRead(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getPCIeRead(zes_device_handle,
                                          [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                              callback(ret, e);
                                          });
}

void GPUDevice::getPCIeWrite(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getPCIeWrite(zes_device_handle,
                                           [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                               callback(ret, e);
                                           });
}

void GPUDevice::getFabricThroughput(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getFabricThroughput(zes_device_handle,
                                                  [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                                      callback(ret, e);
                                                  });
}

void GPUDevice::getPerfMetrics(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getPerfMetrics(zes_device_handle, ze_driver_handle,
                                                  [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                                      callback(ret, e);
                                                  });
}

} // end namespace xpum
