#include "gpu_device.h"

#include <iostream>
#include <fstream>
#include <chrono>

#include "device/gpu/gpu_device_stub.h"
#include "infrastructure/device_property.h"
#include "infrastructure/logger.h"
#include "stdio.h"
#include "unistd.h"


namespace xpum {
using namespace std::chrono_literals;

extern int cmd_firmware(const char* file, unsigned int versions[4]);

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
    GPUDeviceStub::instance().getTemperature(zes_device_handle,
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

bool GPUDevice::runFirmwareFlash(const char* filePath, const std::string& toolPath) noexcept {
    Property pcieAddrProp;
    bool res = getProperty(XPUM_DEVICE_PROPERTY_PCI_BDF_ADDRESS, pcieAddrProp);
    if (!res) {
        return false;
    }

    //remove first "0000"
    std::string address = pcieAddrProp.getValue();
    std::string::size_type begin = address.find(":");
    if (begin == std::string::npos) {
        return false;
    }

    std::string addrForTool = address.substr(begin + 1, address.length());

    //change last "." to ":"
    begin = addrForTool.find(".");
    if (begin == std::string::npos) {
        return false;
    }
    addrForTool[begin] = ':';

    std::string command = toolPath + " -Y -Device " + addrForTool + " -F " + filePath;

    std::lock_guard<std::mutex> lck(mtx);
    if (taskGSC.valid()) {
        return false;
    } else {
        taskGSC = std::async(std::launch::async, [&, command] {
            FILE* commandExec = popen(command.c_str(), "r");
            while (true) {
                xpum_firmware_flash_result_t rc;
                char buf[BUFFERSIZE];
                char* res = fgets(buf, BUFFERSIZE, commandExec);
                if (res != nullptr) {
                    log += std::string{res};
                }
                else {
                    dumpFirmwareFlashLog();
                    log.clear();
                    if (pclose(commandExec) == 0) {
                        rc = xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
                    } else {
                        rc = xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
                    }
                    commandExec = nullptr;
                    return rc;
                }
            }
        } );

        return true;
    }
}

bool GPUDevice::runFirmwareFlash(const char* filePath) noexcept {
    std::lock_guard<std::mutex> lck(mtx);
    if (taskAMC.valid()) {
        return false;
    }
    else {
        std::string dupPath(filePath);
        taskAMC = std::async(std::launch::async, [=] {
            int rc = cmd_firmware(dupPath.c_str(), nullptr);
            if (rc == 0) {
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
            }
            else {
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }
        });

        return true;
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
    if ( type == xpum_firmware_type_t::XPUM_DEVICE_FIRMWARE_GSC ) {
        task = &taskGSC;
    }
    else {
        task = &taskAMC;
    }

    if (task->valid()) { 
        auto status = task->wait_for(0ms);
        if (status == std::future_status::ready) {
            std::lock_guard<std::mutex> lck(mtx);
            return task->get();
        }
        else {
            return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ONGOING;
        }
    } else {
        return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
    }
}

bool GPUDevice::getAMCFirmwareVersion(unsigned int versions[4]) noexcept {
    int rc = cmd_firmware(nullptr, versions);
    return rc == 0 ? true : false;
}

bool GPUDevice::isUpgradingFw(void) noexcept {
    return taskGSC.valid() || taskAMC.valid();
}

} // end namespace xpum
