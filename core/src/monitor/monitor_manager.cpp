#include "monitor_manager.h"

#include <algorithm>

#include "infrastructure/configuration.h"
#include "infrastructure/device_capability.h"
#include "infrastructure/logger.h"
#include "infrastructure/utility.h"
#include "infrastructure/configuration.h"
#include "monitor_task.h"

namespace xpum {

MonitorManager::MonitorManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                               std::shared_ptr<DataLogicInterface>& p_data_logic)
    : p_device_manager(p_device_manager), p_data_logic(p_data_logic) {
    XPUM_LOG_DEBUG("MonitorManager()");
    p_scheduled_thread_pool = std::make_shared<ScheduledThreadPool>(16);
}

MonitorManager::~MonitorManager() {
    XPUM_LOG_DEBUG("~MonitorManager()");
}

void MonitorManager::init() {
    std::unique_lock<std::mutex> lock(this->mutex);

    createMonitorTasks();

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

void MonitorManager::createMonitorTasks() {
    std::vector<MeasurementType> metric_types = Configuration::getEnabledMetrics();
    for (auto& type : metric_types) {
        tasks.emplace_back(std::make_shared<MonitorTask>(Utility::capabilityFromMeasurementType(type), Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS));
    }
}

void MonitorManager::addMetricTask(MeasurementType type, int freq) {
    std::unique_lock<std::mutex> lock(this->mutex);
    if (Utility::isMetric(type)) {
        for (auto& p_task : tasks) {
            if (p_task->getCapability() == Utility::capabilityFromMeasurementType(type)) {
                return;
            }
        }
        auto task = std::make_shared<MonitorTask>(Utility::capabilityFromMeasurementType(type),
                                                  freq, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS);
        tasks.emplace_back(task);
        task->start(this->p_scheduled_thread_pool);
    }
}

void MonitorManager::removeMetricTask(MeasurementType type) {
    std::unique_lock<std::mutex> lock(this->mutex);
    DeviceCapability capability = Utility::capabilityFromMeasurementType(type);
    for (auto& p_task : tasks) {
        if (p_task->getType() == MonitorTaskType::GPU_METRICS && p_task->getCapability() == capability) {
            p_task->stop();
        }
    }
    tasks.erase(std::remove_if(tasks.begin(), tasks.end(), [capability](std::shared_ptr<MonitorTask> t) { return (t->getType() == MonitorTaskType::GPU_METRICS && t->getCapability() == capability); }), tasks.end());
}

void MonitorManager::resetMetricTasksFrequency() {
    std::vector<MeasurementType> metric_types;
    Utility::getMetricsTypes(metric_types);
    std::vector<MeasurementType>::iterator iter = metric_types.begin();
    while (iter != metric_types.end()) {
        removeMetricTask(*iter++);
    }
    
    iter = metric_types.begin();
    while (iter != metric_types.end()) {
        addMetricTask(*iter++, Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE);
    }
}

} // end namespace xpum
