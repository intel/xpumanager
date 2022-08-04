/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file health_manager.h
 */

#pragma once

#include "control/device_manager_interface.h"
#include "data_logic/data_logic_interface.h"
#include "health_data_type.h"
#include "health_manager_interface.h"

namespace xpum {

/**
 * The class is responsible for GPU health check in real time. Four components are currently supported: 
 * power, temperature, memory and fabric port. In addition, users can set reasonable thresholds for power
 * and temperature to suit their needs.
 *
 */

class HealthManager : public HealthManagerInterface {
   public:
    HealthManager(std::shared_ptr<DeviceManagerInterface> &p_device_manager,
                  std::shared_ptr<DataLogicInterface> &p_data_logic);

    virtual ~HealthManager();

   public:
    void init() override;

    void close() override;

    xpum_result_t setHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void *value) override;

    xpum_result_t getHealthConfig(xpum_device_id_t deviceId, xpum_health_config_type_t key, void *value) override;

    xpum_result_t getHealth(xpum_device_id_t deviceId, xpum_health_type_t type, xpum_health_data_t *data) override;

   private:
    uint64_t getThrottlePower(std::string pciDeviceId);

    uint64_t getThrottleCoreTemperature(std::string pciDeviceId);

    uint64_t getShutdownCoreTemperature(std::string pciDeviceId);

    uint64_t getShutdownMemoryTemperature(std::string pciDeviceId);

    std::shared_ptr<DeviceManagerInterface> p_device_manager;

    std::shared_ptr<DataLogicInterface> p_data_logic;

    std::map<xpum_device_id_t, int> p_health_core_thermal_configs;

    std::map<xpum_device_id_t, int> p_health_memory_thermal_configs;

    std::map<xpum_device_id_t, int> p_health_power_configs;

    std::map<uint32_t, uint64_t> p_health_device_to_tdps;

    std::map<uint32_t, uint64_t> p_health_device_to_throttle_core_temperatures;

    std::map<uint32_t, uint64_t> p_health_device_to_shutdown_core_temperatures;

    std::map<uint32_t, uint64_t> p_health_device_to_shutdown_memory_temperatures;

    std::mutex mutex;
};

} // end namespace xpum
