/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file gpu_device.cpp
 */

#include "gpu_device.h"

#include <chrono>
#include <fstream>
#include <iostream>

#include "core/core.h"
#include "device/gpu/gpu_device_stub.h"
#include "group/group_manager.h"
#include "infrastructure/device_property.h"
#include "infrastructure/logger.h"
#include "stdio.h"
#include "unistd.h"

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

void GPUDevice::getActuralFrequency(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getActuralFrequency(zes_device_handle,
                                                  [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                                      callback(ret, e);
                                                  });
}

void GPUDevice::getRequestFrequency(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getRequestFrequency(zes_device_handle,
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

void GPUDevice::getMemory(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getMemory(zes_device_handle,
                                        [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                            callback(ret, e);
                                        });
}

void GPUDevice::getMemoryUtilization(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getMemoryUtilization(zes_device_handle,
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

void GPUDevice::getMemoryRead(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getMemoryRead(zes_device_handle,
                                            [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                                callback(ret, e);
                                            });
}

void GPUDevice::getMemoryWrite(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getMemoryWrite(zes_device_handle,
                                             [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                                 callback(ret, e);
                                             });
}

void GPUDevice::getMemoryReadThroughput(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getMemoryReadThroughput(zes_device_handle,
                                                      [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
                                                          callback(ret, e);
                                                      });
}

void GPUDevice::getMemoryWriteThroughput(Callback_t callback) noexcept {
    GPUDeviceStub::instance().getMemoryWriteThroughput(zes_device_handle,
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

static std::vector<std::string> getSiblingDeviceBDFAddr(GPUDevice* device) {
    xpum::Core& core = xpum::Core::instance();
    auto groupManager = core.getGroupManager();
    xpum_group_id_t groupIds[XPUM_MAX_NUM_GROUPS];
    std::vector<std::string> result;
    Property pcieAddrProp;
    int count;
    groupManager->getAllGroupIds(groupIds, &count);
    xpum_device_id_t deviceId = std::stoi(device->getId());
    for (int i = 0; i < count; i++) {
        auto groupId = groupIds[i];
        if (groupId & BUILD_IN_GROUP_MASK) {
            xpum_group_info_t groupInfo;
            groupManager->getGroupInfo(groupId, &groupInfo);

            for (int j = 0; j < groupInfo.count; j++) {
                // device in build in group
                if (groupInfo.deviceList[j] == deviceId) {
                    auto deviceManager = core.getDeviceManager();
                    for (int k = 0; k < groupInfo.count; k++) {
                        auto siblingId = groupInfo.deviceList[k];
                        std::string siblingIdStr = std::to_string(siblingId);
                        auto pSiblingDevice = deviceManager->getDevice(siblingIdStr);
                        pSiblingDevice->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS, pcieAddrProp);
                        result.push_back(pcieAddrProp.getValue());
                    }
                    return result;
                }
            }
        }
    }
    device->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS, pcieAddrProp);
    result.push_back(pcieAddrProp.getValue());
    return result;
}

xpum_result_t GPUDevice::runFirmwareFlash(const char* filePath, const std::string& toolPath) noexcept {
    auto bdfAddrs = getSiblingDeviceBDFAddr(this);

    std::vector<std::string> commands;

    for (auto address : bdfAddrs) {
        // remove first "0000"
        std::string::size_type begin = address.find(":");
        if (begin == std::string::npos) {
            return xpum_result_t::XPUM_GENERIC_ERROR;
        }

        std::string addrForTool = address.substr(begin + 1, address.length());

        // change last "." to ":"
        begin = addrForTool.find(".");
        if (begin == std::string::npos) {
            return xpum_result_t::XPUM_GENERIC_ERROR;
        }
        addrForTool[begin] = ':';

        std::string command = toolPath + " -Y -Device " + addrForTool + " -F " + filePath;
        commands.push_back(command);
    }
    if (commands.size() == 0) {
        return xpum_result_t::XPUM_GENERIC_ERROR;
    }

    std::lock_guard<std::mutex> lck(mtx);
    if (taskGSC.valid()) {
        // task already running
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
    } else {
        taskGSC = std::async(std::launch::async, [&, commands] {
            bool ok = true;
            for (std::string command : commands) {
                FILE* commandExec = popen(command.c_str(), "r");
                if (!commandExec) {
                    ok = false;
                    break;
                }
                while (true) {
                    char buf[BUFFERSIZE];
                    char* res = fgets(buf, BUFFERSIZE, commandExec);
                    if (res != nullptr) {
                        log += std::string{res};
                    } else {
                        ok = ok && pclose(commandExec) == 0;
                        commandExec = nullptr;
                        break;
                    }
                }
                // break when previous update fail
                if (!ok) break;
            }
            // dumpFirmwareFlashLog();
            log.clear();
            xpum_firmware_flash_result_t rc;
            if (ok) {
                rc = xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
            } else {
                rc = xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }
            return rc;
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

xpum_firmware_flash_result_t GPUDevice::getFirmwareFlashResult(xpum_firmware_type_t type) noexcept {
    std::future<xpum_firmware_flash_result_t>* task;
    if (type == xpum_firmware_type_t::XPUM_DEVICE_FIRMWARE_GSC) {
        task = &taskGSC;
    }
    else {
        return XPUM_DEVICE_FIRMWARE_FLASH_UNSUPPORTED;
    }

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

} // end namespace xpum
