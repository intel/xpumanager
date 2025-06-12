/* 
 *  Copyright (C) 2021-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file core.cpp
 */

#include "core.h"

#include "control/device_manager.h"
#include "data_logic/data_logic.h"
#include "device/gpu/gpu_device_stub.h"
#include "diagnostic/diagnostic_manager.h"
#include "group/group_manager.h"
#include "health/health_manager.h"
#include "infrastructure/configuration.h"
#include "infrastructure/exception/ilegal_state_exception.h"
#include "infrastructure/logger.h"
#include "monitor/monitor_manager.h"
#include "policy/policy_manager.h"
#include "topology/topology.h"

namespace xpum {

Core::Core()
    : p_device_manager(nullptr),
      p_data_logic(nullptr),
      p_monitor_manager(nullptr),
      p_health_manager(nullptr),
      p_diagnostic_manager(nullptr),
      p_policy_manager(nullptr),
      p_dump_raw_data_manager(nullptr),
      initialized(false),
      ze_initialized(false) {
    XPUM_LOG_TRACE("core()");
}

Core::~Core() {
    XPUM_LOG_TRACE("~core()");
}

Core& Core::instance() {
    static Core instance;
    return instance;
}

std::shared_ptr<DeviceManagerInterface> Core::getDeviceManager() {
    return p_device_manager;
}

std::shared_ptr<DataLogicInterface> Core::getDataLogic() {
    return p_data_logic;
}

std::shared_ptr<MonitorManagerInterface> Core::getMonitorManager() {
    return p_monitor_manager;
}

std::shared_ptr<HealthManagerInterface> Core::getHealthManager() {
    return p_health_manager;
}

std::shared_ptr<GroupManagerInterface> Core::getGroupManager() {
    return p_group_manager;
}

std::shared_ptr<DiagnosticManagerInterface> Core::getDiagnosticManager() {
    return p_diagnostic_manager;
}

std::shared_ptr<PolicyManagerInterface> Core::getPolicyManager() {
    return p_policy_manager;
}

std::shared_ptr<DumpRawDataManager> Core::getDumpRawDataManager() {
    return p_dump_raw_data_manager;
}

std::shared_ptr<FirmwareManager> Core::getFirmwareManager() {
    return p_firmware_manager;
}

std::shared_ptr<VgpuManager> Core::getVgpuManager() {
    return p_vgpu_manager;
}

void Core::init() {
    std::unique_lock<std::mutex> lock(mutex);
    if (initialized) {
        return;
    }

    XPUM_LOG_INFO("xpumd core starts to initialize");

    XPUM_LOG_INFO("initialize configuration");
    Configuration::init();

    XPUM_LOG_INFO("initialize datalogic");
    p_data_logic = std::make_shared<DataLogic>();
    p_data_logic->init();

    // Create the instance of FirmwareManger earlier then it may work 
    // even L0 init got failed and FirmwareManager::init was not called
    p_firmware_manager = std::make_shared<FirmwareManager>();

    XPUM_LOG_INFO("initialize device manager");
    p_device_manager = std::make_shared<DeviceManager>(p_data_logic);
    p_device_manager->init();

    XPUM_LOG_INFO("initialize health manager");
    p_health_manager = std::make_shared<HealthManager>(p_device_manager, p_data_logic);
    p_health_manager->init();

    XPUM_LOG_INFO("initialize group manager");
    p_group_manager = std::make_shared<GroupManager>(p_device_manager, p_data_logic);
    p_group_manager->init();

    XPUM_LOG_INFO("initialize policy manager");
    p_policy_manager = std::make_shared<PolicyManager>(p_device_manager, p_data_logic, p_group_manager);
    p_policy_manager->init();

    XPUM_LOG_INFO("initialize dump raw data manager");
    p_dump_raw_data_manager = std::make_shared<DumpRawDataManager>();

    XPUM_LOG_INFO("initialize firmware manager");
    p_firmware_manager->init();

    XPUM_LOG_INFO("initialize diagnostic manager");
    p_diagnostic_manager = std::make_shared<DiagnosticManager>(p_device_manager, p_data_logic, p_firmware_manager);
    p_diagnostic_manager->init();

    XPUM_LOG_INFO("initialize monitor manager");
    p_monitor_manager = std::make_shared<MonitorManager>(p_device_manager, p_data_logic);
    p_monitor_manager->init();

    p_vgpu_manager = std::make_shared<VgpuManager>();

    XPUM_LOG_INFO("xpumd core initialization completed");
    initialized = true;
}

void Core::close() {
    std::unique_lock<std::mutex> lock(mutex);
    if (!initialized) {
        return;
    }

    p_firmware_manager = nullptr;

    p_dump_raw_data_manager = nullptr;

    Topology::clearTopology();

    close(std::dynamic_pointer_cast<InitCloseInterface>(p_policy_manager),
          "Failed to close policy manager");
    close(std::dynamic_pointer_cast<InitCloseInterface>(p_diagnostic_manager),
          "Failed to close diagnostic manager");
    close(std::dynamic_pointer_cast<InitCloseInterface>(p_group_manager),
          "Failed to close group manager");
    close(std::dynamic_pointer_cast<InitCloseInterface>(p_health_manager),
          "Failed to close health manager");
    close(std::dynamic_pointer_cast<InitCloseInterface>(p_monitor_manager),
          "Failed to close monitor manager");
    close(std::dynamic_pointer_cast<InitCloseInterface>(p_device_manager),
          "Failed to close device manager");
    close(std::dynamic_pointer_cast<InitCloseInterface>(p_data_logic),
          "Failed to close data logic");
}

void Core::close(const std::shared_ptr<InitCloseInterface>& p_init_close_interface,
                 const std::string& p_msgPrix) {
    if (p_init_close_interface == nullptr) {
        return;
    }

    try {
        p_init_close_interface->close();
    } catch (std::exception& e) {
        XPUM_LOG_WARN("{}: {}", p_msgPrix, e.what());
    } catch (...) {
        XPUM_LOG_WARN("{}: unexpected exception", p_msgPrix);
    }
}

bool Core::isInitialized() {
    std::unique_lock<std::mutex> lock(mutex);
    return this->initialized;
}

bool Core::isZeInitialized() {
    std::unique_lock<std::mutex> lock(mutex);
    return this->ze_initialized;
}

void Core::setZeInitialized(bool val) {
    std::unique_lock<std::mutex> lock(mutex);
    this->ze_initialized = val;
}

xpum_result_t Core::apiAccessPreCheck() {
    if (!this->ze_initialized) {
        return XPUM_LEVEL_ZERO_INITIALIZATION_ERROR;
    }
    return XPUM_OK;
}

} // end namespace xpum
