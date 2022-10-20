/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file monitor_manager.cpp
 */

#include "monitor_manager.h"

#include <algorithm>
#include <set>

#include "core/core.h"
#include "infrastructure/configuration.h"
#include "infrastructure/device_capability.h"
#include "infrastructure/logger.h"
#include "infrastructure/utility.h"
#include "monitor_task.h"

namespace xpum {

MonitorManager::MonitorManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                               std::shared_ptr<DataLogicInterface>& p_data_logic)
    : p_device_manager(p_device_manager), p_data_logic(p_data_logic) {
    XPUM_LOG_TRACE("MonitorManager()");
    p_scheduled_thread_pool = std::make_shared<ScheduledThreadPool>(16);
}

MonitorManager::~MonitorManager() {
    XPUM_LOG_TRACE("~MonitorManager()");
}

void MonitorManager::init() {
    char* env = std::getenv("XPUM_DISABLE_PERIODIC_METRIC_MONITOR");
    std::string xpum_disable_periodic_metric_monitor{env != NULL ? env : ""};
    if (xpum_disable_periodic_metric_monitor == "1")
        return;

    std::unique_lock<std::mutex> lock(this->mutex);

    createMonitorTasks(MeasurementType::METRIC_MAX);

    for (uint64_t session = 0; session < Configuration::MAX_STATISTICS_SESSION_NUM; session++) {
        std::vector<std::shared_ptr<Device>> devices;
        Core::instance().getDeviceManager()->getDeviceList(devices);
        for (auto device : devices) {
            Core::instance().getDataLogic()->updateStatsTimestamp(session, std::stoi(device->getId()));
            Core::instance().getDataLogic()->updateEngineStatsTimestamp(session, std::stoi(device->getId()));
            Core::instance().getDataLogic()->updateFabricStatsTimestamp(session, std::stoi(device->getId()));
        }
    }

    for (auto& p_task : tasks) {
        p_task->start(this->p_scheduled_thread_pool);
    }
}

void MonitorManager::close() {
    std::unique_lock<std::mutex> lock(this->mutex);
    for (auto& p_task : tasks) {
        p_task->stop();
    }
    tasks.clear();
    if (p_scheduled_thread_pool != nullptr) {
        p_scheduled_thread_pool->close();
        p_scheduled_thread_pool = nullptr;
    }
}

void MonitorManager::createMonitorTasks(MeasurementType target_type) {
    auto metric_types = Configuration::getEnabledMetrics();
    std::set<DeviceCapability> created_caps;
    for (auto& type : metric_types) {
        if (target_type != MeasurementType::METRIC_MAX && type != target_type) {
            continue;
        }
        DeviceCapability capability = Utility::capabilityFromMeasurementType(type);
        if (created_caps.find(capability) == created_caps.end()) {
            tasks.emplace_back(std::make_shared<MonitorTask>(capability, Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS));
            created_caps.emplace(capability);
        }
    }
}

void MonitorManager::resetMetricTasksFrequency() {
    char* env = std::getenv("XPUM_DISABLE_PERIODIC_METRIC_MONITOR");
    std::string xpum_disable_periodic_metric_monitor{env != NULL ? env : ""};
    if (xpum_disable_periodic_metric_monitor == "1") {
        return;
    }
    
    std::unique_lock<std::mutex> lock(this->mutex);
    for (auto& p_task : tasks) {
        p_task->stop();
    }
    tasks.clear();
    createMonitorTasks(MeasurementType::METRIC_MAX);
    for (auto& p_task : tasks) {
        p_task->start(this->p_scheduled_thread_pool);
    }
}

bool MonitorManager::initOneTimeMetricMonitorTasks(MeasurementType type) {
    char* env = std::getenv("XPUM_DISABLE_PERIODIC_METRIC_MONITOR");
    std::string xpum_disable_periodic_metric_monitor{env != NULL ? env : ""};
    if (xpum_disable_periodic_metric_monitor == "1") {
        XPUM_LOG_TRACE("Init One-Time Monitor Tasks");
        std::unique_lock<std::mutex> lock(this->mutex);

        createMonitorTasks(type);

        for (uint64_t session = 0; session < Configuration::MAX_STATISTICS_SESSION_NUM; session++) {
            std::vector<std::shared_ptr<Device>> devices;
            Core::instance().getDeviceManager()->getDeviceList(devices);
            for (auto device : devices) {
                Core::instance().getDataLogic()->updateStatsTimestamp(session, std::stoi(device->getId()));
                Core::instance().getDataLogic()->updateEngineStatsTimestamp(session, std::stoi(device->getId()));
                Core::instance().getDataLogic()->updateFabricStatsTimestamp(session, std::stoi(device->getId()));
            }
        }

        for (auto& p_task : tasks) {
            p_task->start(this->p_scheduled_thread_pool);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE/2));
        for (auto& p_task : tasks) {
            p_task->start(this->p_scheduled_thread_pool);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE/2));

        for (auto& p_task : tasks) {
            p_task->stop();
        }
        tasks.clear();
        return true;
    }
    return false;
}
} // end namespace xpum
