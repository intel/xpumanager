/* 
 *  Copyright (C) 2021-2023 Intel Corporation
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
    p_health_device_to_tdps = {{0x0205, 150}, {0x0203, 150}, {0x020A, 300}, {0x56C0, 150}, {0x56C1, 37.5}, 
                               {0x0BD0, 600}, {0x0BD4, 600}, {0x0BD5, 600}, {0x0BD6, 600}, {0x0BD7, 450}, {0x0BD8, 450},
                               {0x0BD9, 300}, {0x0BDA, 300}, {0x0BDB, 300}, {0x0B6E, 300}, {0x0BE5, 600}, {0x4907, 25}};
    p_health_device_to_throttle_core_temperatures = {{0x56C0, 100}, {0x56C1, 95}};
    p_health_device_to_shutdown_core_temperatures = {{0x56C0, 125}, {0x56C1, 125}};
    p_health_device_to_shutdown_memory_temperatures = {{0x56C0, 105}, {0x56C1, 105}};
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
                p_health_memory_thermal_configs.erase(deviceId);
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
    Property prop;
    std::string pciDeviceId;
    if (this->p_device_manager->getDevice(std::to_string(deviceId))->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_DEVICE_ID, prop)) {
        pciDeviceId = prop.getValue();
    }
    int limit = -1;
    switch (key) {
        case xpum_health_config_type_t::XPUM_HEALTH_CORE_THERMAL_LIMIT:
            limit = getShutdownCoreTemperature(pciDeviceId);
            if (threshold <= 0 || threshold > limit)
                return XPUM_RESULT_HEALTH_INVALID_THRESHOLD;
            p_health_core_thermal_configs[deviceId] = threshold;
            break;
        case xpum_health_config_type_t::XPUM_HEALTH_MEMORY_THERMAL_LIMIT:
            limit = getShutdownMemoryTemperature(pciDeviceId);
            if (threshold <= 0 || threshold > limit)
                return XPUM_RESULT_HEALTH_INVALID_THRESHOLD;
            p_health_memory_thermal_configs[deviceId] = threshold;
            break;
        case xpum_health_config_type_t::XPUM_HEALTH_POWER_LIMIT:
            limit = getThrottlePower(pciDeviceId);
            if (threshold <= 0 || threshold > limit)
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

    Property prop;
    std::string pciDeviceId;
    if (this->p_device_manager->getDevice(std::to_string(deviceId))->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_DEVICE_ID, prop)) {
        pciDeviceId = prop.getValue();
    }
    if (type == xpum_health_type_t::XPUM_HEALTH_CORE_THERMAL) {
        data->throttleThreshold = getThrottleCoreTemperature(pciDeviceId);
        data->shutdownThreshold = getShutdownCoreTemperature(pciDeviceId);
        if (Configuration::XPUM_MODE == "xpu-smi")
            p_health_core_thermal_configs[deviceId] = data->throttleThreshold;
    } else if (type == xpum_health_type_t::XPUM_HEALTH_MEMORY_THERMAL) {
        data->throttleThreshold = 85;
        data->shutdownThreshold = getShutdownMemoryTemperature(pciDeviceId);
        if (Configuration::XPUM_MODE == "xpu-smi")
            p_health_memory_thermal_configs[deviceId] = data->throttleThreshold;
    } else if (type == xpum_health_type_t::XPUM_HEALTH_POWER) {
        data->throttleThreshold = getThrottlePower(pciDeviceId);
        if (Configuration::XPUM_MODE == "xpu-smi")
            p_health_power_configs[deviceId] = data->throttleThreshold;
    } else {
        if (type != xpum_health_type_t::XPUM_HEALTH_MEMORY && type != xpum_health_type_t::XPUM_HEALTH_FABRIC_PORT
            && type != xpum_health_type_t::XPUM_HEALTH_FREQUENCY)
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

uint64_t HealthManager::getThrottlePower(std::string pciDeviceId) {
    uint64_t value = 300; // default throttle power

    if (pciDeviceId.empty()) {
        return value;
    }

    uint32_t id = std::stoi(pciDeviceId, nullptr, 16);

    if (p_health_device_to_tdps.count(id) > 0)
        value = p_health_device_to_tdps[id];

    return value;
}

uint64_t HealthManager::getThrottleCoreTemperature(std::string pciDeviceId) {
    uint64_t value = 105; // default throttle core temperature

    if (pciDeviceId.empty()) {
        return value;
    }

    uint32_t id = std::stoi(pciDeviceId, nullptr, 16);

    if (p_health_device_to_throttle_core_temperatures.count(id) > 0)
        value = p_health_device_to_throttle_core_temperatures[id];

    return value;
}

uint64_t HealthManager::getShutdownCoreTemperature(std::string pciDeviceId) {
    uint64_t value = 130; // default shutdown core temperature

    if (pciDeviceId.empty()) {
        return value;
    }

    uint32_t id = std::stoi(pciDeviceId, nullptr, 16);

    if (p_health_device_to_shutdown_core_temperatures.count(id) > 0)
        value = p_health_device_to_shutdown_core_temperatures[id];

    return value;
}

uint64_t HealthManager::getShutdownMemoryTemperature(std::string pciDeviceId) {
    uint64_t value = 100; // default shutdown memory temperature

    if (pciDeviceId.empty()) {
        return value;
    }

    uint32_t id = std::stoi(pciDeviceId, nullptr, 16);

    if (p_health_device_to_shutdown_memory_temperatures.count(id) > 0)
        value = p_health_device_to_shutdown_memory_temperatures[id];

    return value;
}
} // end namespace xpum
