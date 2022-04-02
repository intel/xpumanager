/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file health_manager.cpp
 */

#include "health_manager.h"

#include <algorithm>

#include "device/gpu/gpu_device_stub.h"
#include "infrastructure/configuration.h"
#include "infrastructure/logger.h"

namespace xpum {

HealthManager::HealthManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                             std::shared_ptr<DataLogicInterface>& p_data_logic)
    : p_device_manager(p_device_manager), p_data_logic(p_data_logic) {
    XPUM_LOG_TRACE("HealthManager()");
    p_health_device_to_tdps = {{"0x0205", 150}, {"0x0203", 150}, {"0x020A", 300}, {"0x56C0", 150}, {"0x56C1", 37.5}, {"0x0BD5", 600}};
}

HealthManager::~HealthManager() {
    XPUM_LOG_TRACE("~HealthManager()");
}

void HealthManager::init() {
}

void HealthManager::close() {
}

xpum_result_t HealthManager::setHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void* value) {
    if (this->p_device_manager->getDevice(std::to_string(deviceId)) == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    std::unique_lock<std::mutex> lock(this->mutex);
    if (value == nullptr || *static_cast<int*>(value) == -1) {
        switch (key) {
            case xpum_health_config_type_t::XPUM_HEALTH_CORE_THERMAL_LIMIT:
                p_health_core_thermal_configs.erase(deviceId);
                break;
            case xpum_health_config_type_t::XPUM_HEALTH_MEMORY_THERMAL_LIMIT:
                p_health_core_thermal_configs.erase(deviceId);
                break;
            case xpum_health_config_type_t::XPUM_HEALTH_POWER_LIMIT:
                p_health_power_configs.erase(deviceId);
                break;
            default:
                return XPUM_RESULT_HEALTH_INVALID_CONIG_TYPE;
        }
        return XPUM_OK;
    }

    int threshold = *static_cast<int*>(value);
    switch (key) {
        case xpum_health_config_type_t::XPUM_HEALTH_CORE_THERMAL_LIMIT:
            if (threshold <= 0 || threshold > 130) // (0, 130]
                return XPUM_RESULT_HEALTH_INVALID_THRESHOLD;
            p_health_core_thermal_configs[deviceId] = threshold;
            break;
        case xpum_health_config_type_t::XPUM_HEALTH_MEMORY_THERMAL_LIMIT:
            if (threshold <= 0 || threshold > 100) // (0, 100])
                return XPUM_RESULT_HEALTH_INVALID_THRESHOLD;
            p_health_memory_thermal_configs[deviceId] = threshold;
            break;
        case xpum_health_config_type_t::XPUM_HEALTH_POWER_LIMIT:
            if (threshold <= 0 || threshold > 600) // (0, 600])
                return XPUM_RESULT_HEALTH_INVALID_THRESHOLD;
            p_health_power_configs[deviceId] = threshold;
            break;
        default:
            return XPUM_RESULT_HEALTH_INVALID_CONIG_TYPE;
    }
    return XPUM_OK;
}

xpum_result_t HealthManager::getHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void* value) {
    if (this->p_device_manager->getDevice(std::to_string(deviceId)) == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    std::unique_lock<std::mutex> lock(this->mutex);
    if (value == nullptr) {
        return XPUM_GENERIC_ERROR;
    }

    int* threshold = static_cast<int*>(value);
    *threshold = -1;

    switch (key) {
        case xpum_health_config_type_t::XPUM_HEALTH_CORE_THERMAL_LIMIT:
            if (p_health_core_thermal_configs.find(deviceId) != p_health_core_thermal_configs.end()) {
                *threshold = p_health_core_thermal_configs.at(deviceId);
            }
            break;
        case xpum_health_config_type_t::XPUM_HEALTH_MEMORY_THERMAL_LIMIT:
            if (p_health_memory_thermal_configs.find(deviceId) != p_health_memory_thermal_configs.end()) {
                *threshold = p_health_memory_thermal_configs.at(deviceId);
            }
            break;
        case xpum_health_config_type_t::XPUM_HEALTH_POWER_LIMIT:
            if (p_health_power_configs.find(deviceId) != p_health_power_configs.end()) {
                *threshold = p_health_power_configs.at(deviceId);
            }
            break;
        default:
            return XPUM_RESULT_HEALTH_INVALID_CONIG_TYPE;
    }
    return XPUM_OK;
}

xpum_result_t HealthManager::getHealth(xpum_device_id_t deviceId, xpum_health_type_t type, xpum_health_data_t* data) {
    if (this->p_device_manager->getDevice(std::to_string(deviceId)) == nullptr) {
        return XPUM_RESULT_DEVICE_NOT_FOUND;
    }

    std::unique_lock<std::mutex> lock(this->mutex);
    data->deviceId = deviceId;
    data->type = type;
    data->status = xpum_health_status_t::XPUM_HEALTH_STATUS_UNKNOWN;

    if (type == xpum_health_type_t::XPUM_HEALTH_CORE_THERMAL) {
        data->throttleThreshold = 105;
        data->shutdownThreshold = 130;
    } else if (type == xpum_health_type_t::XPUM_HEALTH_MEMORY_THERMAL) {
        data->throttleThreshold = 85;
        data->shutdownThreshold = 100;
    } else if (type == xpum_health_type_t::XPUM_HEALTH_POWER) {
        Property prop;
        std::string deviceName;
        if (this->p_device_manager->getDevice(std::to_string(deviceId))->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_NAME, prop)) {
            deviceName = prop.getValue();
        }
        data->throttleThreshold = getThrottlePower(deviceName);
    } else {
        if (type != xpum_health_type_t::XPUM_HEALTH_MEMORY && type != xpum_health_type_t::XPUM_HEALTH_FABRIC_PORT)
            return XPUM_RESULT_HEALTH_INVALID_TYPE;
    }

    bool global_default_limit = true;
    int core_thermal_thresold = Configuration::CORE_TEMPERATURE_HEALTH_DEFAULT_LIMIT;
    if (p_health_core_thermal_configs.find(deviceId) != p_health_core_thermal_configs.end()) {
        core_thermal_thresold = p_health_core_thermal_configs.at(deviceId);
        global_default_limit = false;
    }

    int memory_thermal_thresold = Configuration::MEMORY_TEMPERATURE_HEALTH_DEFAULT_LIMIT;
    if (p_health_memory_thermal_configs.find(deviceId) != p_health_memory_thermal_configs.end()) {
        memory_thermal_thresold = p_health_memory_thermal_configs.at(deviceId);
        global_default_limit = false;
    }

    int power_threshold = Configuration::POWER_HEALTH_DEFAULT_LIMIT;
    if (p_health_power_configs.find(deviceId) != p_health_power_configs.end()) {
        power_threshold = p_health_power_configs.at(deviceId);
        global_default_limit = false;
    }

    GPUDeviceStub::instance().getHealthStatus(
        this->p_device_manager->getDevice(std::to_string(deviceId))->getDeviceHandle(), type, data, core_thermal_thresold, memory_thermal_thresold, power_threshold, global_default_limit);

    return XPUM_OK;
}

uint64_t HealthManager::getThrottlePower(std::string deviceName) {
    uint64_t value = 300; // default throttle power

    if (deviceName.empty()) {
        return value;
    }

    std::transform(deviceName.begin(), deviceName.end(), deviceName.begin(), ::toupper);
    for (auto item : p_health_device_to_tdps) {
        auto pciDeviceId = item.first;
        std::transform(pciDeviceId.begin(), pciDeviceId.end(), pciDeviceId.begin(), ::toupper);
        if (deviceName.find(pciDeviceId) != std::string::npos) {
            value = p_health_device_to_tdps[item.first];
            break;
        }
    }
    return value;
}
} // end namespace xpum
