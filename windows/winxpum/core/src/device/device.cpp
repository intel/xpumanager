/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device.cpp
 */

#include "pch.h"
#include "device.h"
#include "api/device_model.h"

namespace xpum {
    Device::Device() {

    }

    std::string Device::getId() noexcept {
        std::unique_lock<std::mutex> lock(this->mutex);
        return id;
    }

    void Device::getCapability(std::vector<DeviceCapability>& capabilites) noexcept {
        std::unique_lock<std::mutex> lock(this->mutex);
        for (auto& cap : capabilities) {
            capabilites.push_back(cap);
        }
    }

    bool Device::hasCapability(DeviceCapability& cap) noexcept {
        std::unique_lock<std::mutex> lock(this->mutex);
        for (auto& existing_cap : capabilities) {
            if (existing_cap == cap) {
                return true;
            }
        }
        return false;
    }

    void Device::getProperties(std::vector<Property>& properties) noexcept {
        std::unique_lock<std::mutex> lock(this->mutex);
        for (auto& prop : this->properties) {
            properties.push_back(prop);
        }
    }

    bool Device::getProperty(xpum_device_internal_property_name_t name, Property& ret) noexcept {
        std::unique_lock<std::mutex> lock(this->mutex);
        for (auto& prop : this->properties) {
            if (prop.getName() == name) {
                ret.setValue(prop.getValue());
                return true;
            }
        }

        return false;
    }

    void Device::addProperty(Property prop) {
        std::unique_lock<std::mutex> lock(this->mutex);
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            if (prop.getName() == it->getName()) {
                it->setValue(prop.getValue());
                return;
            }
        }

        this->properties.push_back(prop);
    }

    int Device::getDeviceModel() {
        Property prop;
        bool res = getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_DEVICE_ID, prop);
        if (!res)
            return XPUM_DEVICE_MODEL_UNKNOWN;
        std::string strValue = prop.getValue();
        std::string stripped = strValue.substr(2); // remove 0x prefix
        int deviceId = std::stoi(stripped, nullptr, 16);
        return getDeviceModelByPciDeviceId(deviceId);
    }
}