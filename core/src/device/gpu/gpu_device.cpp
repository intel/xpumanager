/* 
 *  Copyright (C) 2021-2025 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file gpu_device.cpp
 */

#include "gpu_device.h"

#include <chrono>
#include <fstream>
#include <sstream>
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
    GPUDeviceStub::instance().getActuralRequestFrequency(ze_device_handle, zes_device_handle,
                                                  [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                                      callback(ret, e);
                                                  });
}

void GPUDevice::getTemperature(Callback_t callback, zes_temp_sensors_t type) noexcept {
    GPUDeviceStub::instance().getTemperature(
        ze_device_handle,
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

void GPUDevice::getMemoryThroughputAndBandwidth(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getMemoryThroughputAndBandwidth(zes_device_handle,
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

std::vector<std::string> getRc6PathList(GPUDevice* pDevice) {
    std::vector<std::string> pathList;
    if (!pDevice)
        return pathList;
    Property drm_device, tile_count;
    pDevice->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_DRM_DEVICE, drm_device);
    pDevice->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_TILES, tile_count);

    std::string drm_device_str = drm_device.getValue();

    XPUM_LOG_TRACE("Device {} drm device: {}", pDevice->getId(), drm_device_str);

    if (drm_device_str.empty())
        return pathList;

    for (int i = 0; i < tile_count.getValueInt(); i++) {
        std::string path = "/sys/class/drm/" + drm_device_str.substr(9) + "/gt/gt" + std::to_string(i) + "/rc6_enable";
        pathList.push_back(path);
        XPUM_LOG_TRACE("Device {} rc6_enable file path: {}", pDevice->getId(), path);
    }

    return pathList;
}

bool readRc6(GPUDevice* pDevice, std::vector<int>& valueList, bool& rc6Enabled) {
    auto pathList = getRc6PathList(pDevice);
    if (pathList.empty()) {
        XPUM_LOG_ERROR("Fail to get rc6_enable path for device {}", pDevice->getId());
        return false;
    }
    valueList.clear();
    bool _rc6Enabled = false;
    for (auto path : pathList) {
        int val;
        std::fstream rc6(path, std::ios_base::in);
        if (!rc6) {
            XPUM_LOG_ERROR("Fail to open {}", path);
            return false;
        }
        if (rc6 >> val) {
            XPUM_LOG_INFO("Get {} value: {}", path, val);
        } else {
            XPUM_LOG_ERROR("Fail to read rc6_enable value from: {}", path);
            return false;
        }
        valueList.push_back(val);
        _rc6Enabled = _rc6Enabled || val;
    }
    rc6Enabled = _rc6Enabled;
    return true;
}

bool writeRc6(GPUDevice* pDevice, std::vector<int>& valueList) {
    auto pathList = getRc6PathList(pDevice);
    if (pathList.empty()) {
        XPUM_LOG_ERROR("Fail to get rc6_enable path for device {}", pDevice->getId());
        return false;
    }

    if (valueList.size() != pathList.size()) {
        XPUM_LOG_ERROR("Rc6 value count {} mismatch rc6 file count {} of device {}", valueList.size(), pathList.size(), pDevice->getId());
        return false;
    }

    for (size_t i = 0; i < valueList.size(); i++) {
        auto path = pathList.at(i);
        int val = valueList.at(i);
        std::fstream rc6(path, std::ios_base::out);
        if (!rc6) {
            XPUM_LOG_ERROR("Fail to open {}", path);
            return false;
        }
        if (rc6 << val) {
            XPUM_LOG_INFO("Write {} to {}", val, path);
        } else {
            XPUM_LOG_ERROR("Fail to write {} to {}", val, path);
            return false;
        }
    }
    return true;
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
                flashFwErrMsg = "Cannot initialize device: " + meiPath + ". " + print_device_fw_status(&handle);
                XPUM_LOG_ERROR("Cannot initialize device: {}. {}", meiPath, print_device_fw_status(&handle));
                (void)igsc_device_close(&handle);
                unlock();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }

            bool rc6Enabled = false;
            std::vector<int> valueList;
            if (this->getDeviceModel() == XPUM_DEVICE_MODEL_PVC) {
                // disable rc6 when updating PVC firmware
                if (readRc6(this, valueList, rc6Enabled)) {
                    if (rc6Enabled) {
                        std::vector<int> zeros(valueList.size(), 0);
                        writeRc6(this, zeros);
                    }
                } else {
                    rc6Enabled = false;
                    valueList.clear();
                }
            }

            ret = igsc_device_fw_update_ex(&handle, (const uint8_t*)img.data(), img.size(),
                                           progress_func, this, flags);

            if (rc6Enabled && this->getDeviceModel() == XPUM_DEVICE_MODEL_PVC) {
                writeRc6(this, valueList);
            }

            if (ret){
                flashFwErrMsg = "Update process failed. " + print_device_fw_status(&handle);
                XPUM_LOG_ERROR("Update process failed. {}", print_device_fw_status(&handle));
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
    GPUDeviceStub::instance().getPerfMetrics(ze_device_handle, ze_driver_handle,
                                                  [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                                      callback(ret, e);
                                                  });
}

} // end namespace xpum
