/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device_manager.cpp
 */

#include "device_manager.h"

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <vector>

#include "device/gpu/gpu_device_stub.h"
#include "infrastructure/device_process.h"
#include "infrastructure/exception/ilegal_parameter_exception.h"
#include "infrastructure/handle_lock.h"
#include "infrastructure/logger.h"
#include "infrastructure/utility.h"
#include "level_zero/zes_api.h"

namespace xpum {

DeviceManager::DeviceManager(std::shared_ptr<DataLogicInterface>& p_data_logic)
    : p_data_logic(p_data_logic) {
    XPUM_LOG_TRACE("DeviceManager()");
}

DeviceManager::~DeviceManager() {
    close();
    XPUM_LOG_TRACE("~DeviceManager()");
}

void DeviceManager::init() {
    std::unique_lock<std::mutex> lock(this->mutex);
    std::condition_variable cv;
    std::atomic<bool> ready(false);
    std::weak_ptr<DeviceManager> this_weak_ptr = shared_from_this();

    GPUDeviceStub::instance().discoverDevices([&cv, &ready, this_weak_ptr](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
        auto p_this = this_weak_ptr.lock();
        if (p_this == nullptr) {
            return;
        }

        if (e != nullptr) {
            XPUM_LOG_ERROR("Failed to init device list: {}", e->what());
            ready = true;
            cv.notify_all();
            return;
        }

        if (ret != nullptr) {
            auto p_devices = std::static_pointer_cast<std::vector<std::shared_ptr<Device>>>(ret);

            for (auto& p_device : *p_devices) {
                p_this->devices.emplace_back(p_device);
            }
        }

        ready = true;
        cv.notify_all();
    });

    while (!ready) {
        cv.wait(lock);
    }

    discoverFabricLinks();
}

void DeviceManager::close() {
}

void DeviceManager::getDeviceList(std::vector<std::shared_ptr<Device>>& devices) {
    std::unique_lock<std::mutex> lock(this->mutex);
    for (auto& p_device : this->devices) {
        devices.emplace_back(p_device);
    }
}

void DeviceManager::getDeviceList(
    DeviceCapability cap, std::vector<std::shared_ptr<Device>>& devices) {
    std::unique_lock<std::mutex> lock(this->mutex);
    for (auto& p_device : this->devices) {
        if (p_device->hasCapability(cap)) {
            devices.emplace_back(p_device);
        }
    }
}

MeasurementData DeviceManager::getRealtimeMeasurementData(
    MeasurementType type, std::string& device_id) {
    std::shared_ptr<Device> p_device = getDevice(device_id);
    if (p_device == nullptr) {
        throw IlegalParameterException("device does not exist");
    }

    DeviceCapability capability = Utility::capabilityFromMeasurementType(type);
    auto method = Device::getDeviceMethod(capability, p_device.get());
    if (method == nullptr) {
        throw IlegalParameterException("method does not exist");
    }

    std::shared_ptr<MeasurementData> p_data;
    std::shared_ptr<BaseException> exception;
    std::condition_variable data_cv;
    std::mutex data_mutex;
    std::atomic<bool> ready(false);
    std::weak_ptr<DeviceManager> this_weak_ptr = shared_from_this();

    method([p_device, this_weak_ptr, &p_data, &data_cv, &exception, &ready](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) mutable {
        auto p_this = this_weak_ptr.lock();
        if (p_this == nullptr) {
            return;
        }

        if (e == nullptr && ret != nullptr) {
            p_data = std::static_pointer_cast<MeasurementData>(ret);
        } else if (e != nullptr) {
            exception = e;
        }

        ready = true;
        data_cv.notify_all();
    });

    std::unique_lock<std::mutex> lock(data_mutex);
    while (!ready) {
        data_cv.wait(lock);
    }

    if (exception != nullptr) {
        throw(*exception);
    }

    auto subdeviceAdditionalCurrentDataTypes = p_data->getSubdeviceAdditionalCurrentDataTypes();
    if (subdeviceAdditionalCurrentDataTypes.find(type) != subdeviceAdditionalCurrentDataTypes.end()) {
        auto mData = std::make_shared<MeasurementData>();
        auto subdeviceAdditionalCurrentDatas = p_data->getSubdeviceAdditionalCurrentDatas();
        for (auto& sData : subdeviceAdditionalCurrentDatas) {
            if (sData.first == UINT32_MAX)
                mData->setCurrent(sData.second[type]);
            else
                mData->setSubdeviceDataCurrent(sData.first, sData.second[type]);
        }
        return *mData;
    }
    return *p_data;
}

std::shared_ptr<Device> DeviceManager::getDevice(const std::string& id) {
    std::unique_lock<std::mutex> lock(this->mutex);
    for (auto& p_device : this->devices) {
        if (p_device->getId() == id) {
            return p_device;
        }
    }

    return nullptr;
}

void DeviceManager::getDeviceSchedulers(const std::string& id, std::vector<Scheduler>& schedulers) {
    std::unique_lock<std::mutex> lock(this->mutex);
    zes_device_handle_t device = getDeviceHandle(id);
    GPUDeviceStub::instance().getSchedulers(device, schedulers);
}

void DeviceManager::getDeviceStandbys(const std::string& id, std::vector<Standby>& standbys) {
    std::unique_lock<std::mutex> lock(this->mutex);
    GPUDeviceStub::instance().getStandbys(getDeviceHandle(id), standbys);
}

void DeviceManager::getDevicePowerProps(const std::string& id, std::vector<Power>& powers) {
    std::unique_lock<std::mutex> lock(this->mutex);
    GPUDeviceStub::instance().getPowerProps(getDeviceHandle(id), powers);
}

void DeviceManager::getDevicePowerLimits(const std::string& id,
                                         Power_sustained_limit_t& sustained_limit,
                                         Power_burst_limit_t& burst_limit,
                                         Power_peak_limit_t& peak_limit) {
    std::unique_lock<std::mutex> lock(this->mutex);
    GPUDeviceStub::instance().getPowerLimits(getDeviceHandle(id), sustained_limit, burst_limit, peak_limit);
}

bool DeviceManager::setDevicePowerSustainedLimits(const std::string& id, int32_t tileId,
                                                  const Power_sustained_limit_t& sustained_limit) {
    std::unique_lock<std::mutex> lock(this->mutex);
    return GPUDeviceStub::instance().setPowerSustainedLimits(getDeviceHandle(id), tileId, sustained_limit);
}

bool DeviceManager::setDevicePowerBurstLimits(const std::string& id,
                                              const Power_burst_limit_t& burst_limit) {
    std::unique_lock<std::mutex> lock(this->mutex);
    return GPUDeviceStub::instance().setPowerBurstLimits(getDeviceHandle(id), burst_limit);
}

bool DeviceManager::setDevicePowerPeakLimits(const std::string& id,
                                             const Power_peak_limit_t& peak_limit) {
    std::unique_lock<std::mutex> lock(this->mutex);
    return GPUDeviceStub::instance().setPowerPeakLimits(getDeviceHandle(id), peak_limit);
}

void DeviceManager::getDeviceFrequencyRanges(const std::string& id,
                                             std::vector<Frequency>& frequencies) {
    std::unique_lock<std::mutex> lock(this->mutex);
    GPUDeviceStub::instance().getFrequencyRanges(getDeviceHandle(id), frequencies);
}

zes_device_handle_t DeviceManager::getDeviceHandle(const std::string& id) {
    for (auto& p_device : this->devices) {
        if (p_device->getId() == id) {
            return p_device->getDeviceHandle();
        }
    }
    return nullptr;
}

bool DeviceManager::setDeviceFrequencyRange(const std::string& id,
                                            const Frequency& freq) {
    std::unique_lock<std::mutex> lock(this->mutex);
    return GPUDeviceStub::instance().setFrequencyRange(getDeviceHandle(id), freq);
}

void DeviceManager::getFreqAvailableClocks(const std::string& id, uint32_t subdevice_id, std::vector<double>& clocks) {
    std::unique_lock<std::mutex> lock(this->mutex);
    GPUDeviceStub::instance().getFreqAvailableClocks(getDeviceHandle(id), subdevice_id, clocks);
}

void DeviceManager::getDeviceProcessState(const std::string& id, std::vector<device_process>& processes) {
    std::unique_lock<std::mutex> lock(this->mutex);
    GPUDeviceStub::instance().getDeviceProcessState(getDeviceHandle(id), processes);
}

void DeviceManager::getPerformanceFactor(const std::string& id, std::vector<PerformanceFactor>& pf) {
    std::unique_lock<std::mutex> lock(this->mutex);
    GPUDeviceStub::instance().getPerformanceFactor(getDeviceHandle(id), pf);
}

bool DeviceManager::setPerformanceFactor(const std::string& id, PerformanceFactor& pf) {
    std::unique_lock<std::mutex> lock(this->mutex);
    return GPUDeviceStub::instance().setPerformanceFactor(getDeviceHandle(id), pf);
}

bool DeviceManager::setDeviceStandby(const std::string& id,
                                     const Standby& standby) {
    std::unique_lock<std::mutex> lock(this->mutex);
    return GPUDeviceStub::instance().setStandby(getDeviceHandle(id), standby);
}

bool DeviceManager::setDeviceSchedulerTimeoutMode(const std::string& id,
                                                  const SchedulerTimeoutMode& mode) {
    std::unique_lock<std::mutex> lock(this->mutex);
    return GPUDeviceStub::instance().setSchedulerTimeoutMode(getDeviceHandle(id), mode);
}

bool DeviceManager::setDeviceSchedulerTimesliceMode(const std::string& id,
                                                    const SchedulerTimesliceMode& mode) {
    std::unique_lock<std::mutex> lock(this->mutex);
    return GPUDeviceStub::instance().setSchedulerTimesliceMode(getDeviceHandle(id), mode);
}

bool DeviceManager::setDeviceSchedulerExclusiveMode(const std::string& id, const SchedulerExclusiveMode& mode) {
    std::unique_lock<std::mutex> lock(this->mutex);
    return GPUDeviceStub::instance().setSchedulerExclusiveMode(getDeviceHandle(id), mode);
}

bool DeviceManager::resetDevice(const std::string& id, bool force) {
    std::unique_lock<std::mutex> lock(this->mutex);
    return GPUDeviceStub::instance().resetDevice(getDeviceHandle(id), (ze_bool_t)force);
}

bool DeviceManager::getFabricPorts(const std::string& id, std::vector<port_info>& portInfo) {
    std::unique_lock<std::mutex> lock(this->mutex);
    return GPUDeviceStub::instance().getFabricPorts(getDeviceHandle(id), portInfo);
}

bool DeviceManager::setFabricPorts(const std::string& id, const port_info_set& portInfoSet) {
    std::unique_lock<std::mutex> lock(this->mutex);
    return GPUDeviceStub::instance().setFabricPorts(getDeviceHandle(id), portInfoSet);
}

bool DeviceManager::getEccState(const std::string& id, MemoryEcc& ecc) {
    std::unique_lock<std::mutex> lock(this->mutex);
    return GPUDeviceStub::instance().getEccState(getDeviceHandle(id), ecc);
}

bool DeviceManager::setEccState(const std::string& id, ecc_state_t& newState, MemoryEcc& ecc) {
    std::unique_lock<std::mutex> lock(this->mutex);
    return GPUDeviceStub::instance().setEccState(getDeviceHandle(id), newState, ecc);
}

std::string DeviceManager::getDeviceIDByFabricID(uint64_t fabric_id) {
    std::string ret;
    if (fabric_ids.find(fabric_id) != fabric_ids.end()) {
        ret = fabric_ids[fabric_id];
    }
    return ret;
}

void DeviceManager::discoverFabricLinks() {
    for (auto& p_device : this->devices) {
        zes_device_handle_t device = p_device->getDeviceHandle();
        uint32_t fabric_port_count = 0;
        std::shared_ptr<FabricMeasurementData> ret = std::make_shared<FabricMeasurementData>();
        ze_result_t res;
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFabricPorts(device, &fabric_port_count, nullptr));
        if (res == ZE_RESULT_SUCCESS) {
            std::vector<zes_fabric_port_handle_t> fabric_ports(fabric_port_count);
            XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFabricPorts(device, &fabric_port_count, fabric_ports.data()));
            if (res == ZE_RESULT_SUCCESS) {
                for (auto& fp : fabric_ports) {
                    zes_fabric_port_properties_t props;
                    XPUM_ZE_HANDLE_LOCK(fp, res = zesFabricPortGetProperties(fp, &props));
                    if (res == ZE_RESULT_SUCCESS) {
                        zes_fabric_port_state_t state;
                        XPUM_ZE_HANDLE_LOCK(fp, res = zesFabricPortGetState(fp, &state));
                        if (state.status == ZES_FABRIC_PORT_STATUS_HEALTHY || state.status == ZES_FABRIC_PORT_STATUS_DEGRADED) {
                            fabric_ids[props.portId.fabricId] = p_device->getId();
                            p_device->setFabricID(props.portId.fabricId);
                            p_device->addFabricPortHandle(props.portId.attachId, state.remotePortId.fabricId, state.remotePortId.attachId, fp);
                        }
                    }
                }
            }
        }
    }
}

} // end namespace xpum
