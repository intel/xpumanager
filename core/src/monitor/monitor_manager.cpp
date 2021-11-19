#include "monitor_manager.h"

#include <algorithm>

#include "infrastructure/configuration.h"
#include "infrastructure/device_capability.h"
#include "infrastructure/logger.h"
#include "infrastructure/utility.h"
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
        p_task->start();
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
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_TEMPERATURE,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_FREQUENCY,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_POWER,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_ENERGY,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_COMPUTATION,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_MEMORY_USED,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_MEMORY_UTILIZATION,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_MEMORY_BANDWIDTH,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_MEMORY_READ,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_MEMORY_WRITE,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_MEMORY_READ_THROUGHPUT,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_MEMORY_WRITE_THROUGHPUT,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_ENGINE_GROUP_3D_ALL_UTILIZATION,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_EU_ACTIVE_STALL_IDLE,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_RAS_ERROR_CAT_RESET,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_REQUEST_FREQUENCY,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_MEMORY_TEMPERATURE,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
    tasks.emplace_back(std::make_shared<MonitorTask>(DeviceCapability::METRIC_FREQUENCY_THROTTLE,
                                                     Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool));
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
                                                  freq, p_device_manager, p_data_logic, MonitorTaskType::GPU_METRICS, p_scheduled_thread_pool);
        tasks.emplace_back(task);
        task->start();
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

void MonitorManager::resetMetricTasksFrequency(int freq) {
    std::vector<MeasurementType> metric_types;
    Utility::getMetricsTypes(metric_types);
    std::vector<MeasurementType>::iterator iter = metric_types.begin();
    while (iter != metric_types.end()) {
        removeMetricTask(*iter++);
    }
    Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE = freq;
    iter = metric_types.begin();
    while (iter != metric_types.end()) {
        addMetricTask(*iter++, Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE);
    }
}

} // end namespace xpum
