/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device_manager.cpp
 */

#include "pch.h"
#include "device_manager.h"

#include "device/gpu/gpu_device_stub.h"
#include "infrastructure/logger.h"


namespace xpum {

    DeviceManager::DeviceManager() {
        XPUM_LOG_TRACE("DeviceManager()");
    }

    DeviceManager::~DeviceManager() {
        XPUM_LOG_TRACE("~DeviceManager()");
    }

    void DeviceManager::init() {
        auto p_devices = GPUDeviceStub::instance().toDiscover();

        for (auto& p_device : *p_devices) {
            this->devices.emplace_back(p_device);
        }
    }

    void DeviceManager::close() {
    
    }

    void DeviceManager::getDeviceList(std::vector<std::shared_ptr<Device>>& devices) {
        std::unique_lock<std::mutex> lock(this->mutex);
        for (auto& p_device : this->devices) {
            devices.emplace_back(p_device);
        }
    }

    void DeviceManager::getsubDeviceList(const std::string& id, std::vector<int32_t>&subDeviceList) {
        std::unique_lock<std::mutex> lock(this->mutex);
        for (auto& p_device : this->devices) {
            if (p_device->getId() == id) {
                p_device->getsubDeviceList(subDeviceList);
            }
        }
    }

    void DeviceManager::getDeviceSusPower(const std::string& id, int32_t& Sus_power, bool& Sus_supported) {
        std::unique_lock<std::mutex> lock(this->mutex);
        for (auto& p_device : this->devices) {
            if (p_device->getId() == id) {
                p_device->getDeviceSusPower(Sus_power, Sus_supported);
            }
        }
        return;
    }

    bool DeviceManager::setDevicePowerSustainedLimits(const std::string& id, int powerLimit) {
        std::unique_lock<std::mutex> lock(this->mutex);
        for (auto& p_device : this->devices) {
            if (p_device->getId() == id) {
                bool ret = p_device->setDevicePowerSustainedLimits(powerLimit);
                return ret;
            }
        }
        return false;
    }

    void DeviceManager::getDevicePowerMaxLimit(const std::string& id, int32_t& max_limit, bool& supported) {
        std::unique_lock<std::mutex> lock(this->mutex);
        for (auto& p_device : this->devices) {
            if (p_device->getId() == id) {
                p_device->getDevicePowerMaxLimit(max_limit, supported);
            }
        }
        return;
    }

    void DeviceManager::getDeviceFrequencyRange(const std::string& id, int32_t tileId, double& min, double& max, std::string& clocks, bool& supported) {
        std::unique_lock<std::mutex> lock(this->mutex);
        for (auto& p_device : this->devices) {
            if (p_device->getId() == id) {
                p_device->getDeviceFrequencyRange(tileId, min, max, clocks, supported);
            }
        }
        return;
    }

    bool DeviceManager::setDeviceFrequencyRange(const std::string& id, int32_t tileId, double min, double max) {
        std::unique_lock<std::mutex> lock(this->mutex);
        for (auto& p_device : this->devices) {
            if (p_device->getId() == id) {
                bool ret = p_device->setDeviceFrequencyRange(tileId, min, max);
                return ret;
            }
        }
        return false;
    }

    void DeviceManager::getSimpleEccState(const std::string& id, uint8_t& current, uint8_t& pending) {
        std::unique_lock<std::mutex> lock(this->mutex);
        for (auto& p_device : this->devices) {
            if (p_device->getId() == id) {
                p_device->getSimpleEccState(current, pending);
            }
        }
        return;
    }
    std::shared_ptr<Device> DeviceManager::getDevicebyBDF(const std::string& bdf) {
        std::unique_lock<std::mutex> lock(this->mutex);
        for (auto& p_device : this->devices) {
            std::vector<Property> properties;
            p_device->getProperties(properties);
            for (Property& prop : properties) {
                if (prop.getName() == XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS
                    && prop.getValue() == bdf) {
                    return p_device;
                }
            }
        }

        return nullptr;
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
} // end namespace xpum
