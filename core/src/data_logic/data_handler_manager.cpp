/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file raw_data_manager.cpp
 */

#include "data_handler_manager.h"

#include <algorithm>

#include "engine_group_utilization_data_handler.h"
#include "engine_utilization_data_handler.h"
#include "fabric_throughput_data_handler.h"
#include "gpu_utilization_data_handler.h"
#include "infrastructure/configuration.h"
#include "counter_data_handler.h"
#include "stats_data_handler.h"
#include "time_weighted_average_data_handler.h"
#include "shared_data.h"
#include "perf_metrics_data_handler.h"
#include "device/gpu/gpu_device_stub.h"

namespace xpum {

DataHandlerManager::DataHandlerManager(std::shared_ptr<Persistency>& persistency)
    : p_persistency(persistency) {
}

DataHandlerManager::~DataHandlerManager() {
    close();
}

void DataHandlerManager::init() {
    std::unique_lock<std::mutex> lock(mutex);

    data_handlers[MeasurementType::METRIC_TEMPERATURE] =
        std::make_shared<StatsDataHandler>(MeasurementType::METRIC_TEMPERATURE, p_persistency);
    data_handlers[MeasurementType::METRIC_TEMPERATURE]->init();

    data_handlers[MeasurementType::METRIC_FREQUENCY] =
        std::make_shared<StatsDataHandler>(MeasurementType::METRIC_FREQUENCY, p_persistency);
    data_handlers[MeasurementType::METRIC_FREQUENCY]->init();

    data_handlers[MeasurementType::METRIC_MEDIA_ENGINE_FREQUENCY] =
        std::make_shared<StatsDataHandler>(MeasurementType::METRIC_MEDIA_ENGINE_FREQUENCY, p_persistency);
    data_handlers[MeasurementType::METRIC_MEDIA_ENGINE_FREQUENCY]->init();

    data_handlers[MeasurementType::METRIC_REQUEST_FREQUENCY] =
        std::make_shared<StatsDataHandler>(MeasurementType::METRIC_REQUEST_FREQUENCY, p_persistency);
    data_handlers[MeasurementType::METRIC_REQUEST_FREQUENCY]->init();

    data_handlers[MeasurementType::METRIC_POWER] =
        std::make_shared<TimeWeightedAverageDataHandler>(MeasurementType::METRIC_POWER, p_persistency);
    data_handlers[MeasurementType::METRIC_POWER]->init();

    data_handlers[MeasurementType::METRIC_ENERGY] =
        std::make_shared<StatsDataHandler>(MeasurementType::METRIC_ENERGY, p_persistency);
    data_handlers[MeasurementType::METRIC_ENERGY]->init();

    data_handlers[MeasurementType::METRIC_MEMORY_USED] =
        std::make_shared<StatsDataHandler>(MeasurementType::METRIC_MEMORY_USED, p_persistency);
    data_handlers[MeasurementType::METRIC_MEMORY_USED]->init();

    data_handlers[MeasurementType::METRIC_MEMORY_UTILIZATION] =
        std::make_shared<StatsDataHandler>(MeasurementType::METRIC_MEMORY_UTILIZATION, p_persistency);
    data_handlers[MeasurementType::METRIC_MEMORY_UTILIZATION]->init();

    data_handlers[MeasurementType::METRIC_MEMORY_BANDWIDTH] =
        std::make_shared<TimeWeightedAverageDataHandler>(MeasurementType::METRIC_MEMORY_BANDWIDTH, p_persistency);
    data_handlers[MeasurementType::METRIC_MEMORY_BANDWIDTH]->init();

    data_handlers[MeasurementType::METRIC_MEMORY_READ] =
        std::make_shared<CounterDataHandler>(MeasurementType::METRIC_MEMORY_READ, p_persistency);
    data_handlers[MeasurementType::METRIC_MEMORY_READ]->init();

    data_handlers[MeasurementType::METRIC_MEMORY_WRITE] =
        std::make_shared<CounterDataHandler>(MeasurementType::METRIC_MEMORY_WRITE, p_persistency);
    data_handlers[MeasurementType::METRIC_MEMORY_WRITE]->init();

    data_handlers[MeasurementType::METRIC_MEMORY_READ_THROUGHPUT] =
        std::make_shared<TimeWeightedAverageDataHandler>(MeasurementType::METRIC_MEMORY_READ_THROUGHPUT, p_persistency);
    data_handlers[MeasurementType::METRIC_MEMORY_READ_THROUGHPUT]->init();

    data_handlers[MeasurementType::METRIC_MEMORY_WRITE_THROUGHPUT] =
        std::make_shared<TimeWeightedAverageDataHandler>(MeasurementType::METRIC_MEMORY_WRITE_THROUGHPUT, p_persistency);
    data_handlers[MeasurementType::METRIC_MEMORY_WRITE_THROUGHPUT]->init();

    data_handlers[MeasurementType::METRIC_ENGINE_UTILIZATION] =
        std::make_shared<EngineUtilizationDataHandler>(MeasurementType::METRIC_ENGINE_UTILIZATION, p_persistency);
    data_handlers[MeasurementType::METRIC_ENGINE_UTILIZATION]->init();

    data_handlers[MeasurementType::METRIC_COMPUTATION] =
        std::make_shared<GPUUtilizationDataHandler>(MeasurementType::METRIC_COMPUTATION, p_persistency);
    data_handlers[MeasurementType::METRIC_COMPUTATION]->init();

    data_handlers[MeasurementType::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION] =
        std::make_shared<EngineGroupUtilizationDataHandler>(MeasurementType::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION, p_persistency);
    data_handlers[MeasurementType::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION]->init();

    data_handlers[MeasurementType::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION] =
        std::make_shared<EngineGroupUtilizationDataHandler>(MeasurementType::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION, p_persistency);
    data_handlers[MeasurementType::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION]->init();

    data_handlers[MeasurementType::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION] =
        std::make_shared<EngineGroupUtilizationDataHandler>(MeasurementType::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION, p_persistency);
    data_handlers[MeasurementType::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION]->init();

    data_handlers[MeasurementType::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION] =
        std::make_shared<EngineGroupUtilizationDataHandler>(MeasurementType::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION, p_persistency);
    data_handlers[MeasurementType::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION]->init();

    data_handlers[MeasurementType::METRIC_ENGINE_GROUP_3D_ALL_UTILIZATION] =
        std::make_shared<EngineGroupUtilizationDataHandler>(MeasurementType::METRIC_ENGINE_GROUP_3D_ALL_UTILIZATION, p_persistency);
    data_handlers[MeasurementType::METRIC_ENGINE_GROUP_3D_ALL_UTILIZATION]->init();

    data_handlers[MeasurementType::METRIC_EU_ACTIVE] =
        std::make_shared<StatsDataHandler>(MeasurementType::METRIC_EU_ACTIVE, p_persistency);
    data_handlers[MeasurementType::METRIC_EU_ACTIVE]->init();

    data_handlers[MeasurementType::METRIC_EU_STALL] =
        std::make_shared<StatsDataHandler>(MeasurementType::METRIC_EU_STALL, p_persistency);
    data_handlers[MeasurementType::METRIC_EU_STALL]->init();

    data_handlers[MeasurementType::METRIC_EU_IDLE] =
        std::make_shared<StatsDataHandler>(MeasurementType::METRIC_EU_IDLE, p_persistency);
    data_handlers[MeasurementType::METRIC_EU_IDLE]->init();

    //METRIC_RAS_ERROR
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_RESET] = std::make_shared<StatsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_RESET, p_persistency);
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_RESET]->init();
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS] = std::make_shared<StatsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS, p_persistency);
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS]->init();
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS] = std::make_shared<StatsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS, p_persistency);
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS]->init();
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE] = std::make_shared<StatsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE, p_persistency);
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE]->init();
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE] = std::make_shared<StatsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE, p_persistency);
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE]->init();
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE] = std::make_shared<StatsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE, p_persistency);
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE]->init();
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE] = std::make_shared<StatsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE, p_persistency);
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE]->init();
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE] = std::make_shared<StatsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE, p_persistency);
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE]->init();
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE] = std::make_shared<StatsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE, p_persistency);
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE]->init();

    data_handlers[MeasurementType::METRIC_MEMORY_TEMPERATURE] =
        std::make_shared<StatsDataHandler>(MeasurementType::METRIC_MEMORY_TEMPERATURE, p_persistency);
    data_handlers[MeasurementType::METRIC_MEMORY_TEMPERATURE]->init();

    data_handlers[MeasurementType::METRIC_FREQUENCY_THROTTLE] =
        std::make_shared<TimeWeightedAverageDataHandler>(MeasurementType::METRIC_FREQUENCY_THROTTLE, p_persistency);
    data_handlers[MeasurementType::METRIC_FREQUENCY_THROTTLE]->init();

    data_handlers[MeasurementType::METRIC_FREQUENCY_THROTTLE_REASON_GPU] =
        std::make_shared<StatsDataHandler>(MeasurementType::METRIC_FREQUENCY_THROTTLE_REASON_GPU, p_persistency);

    data_handlers[MeasurementType::METRIC_PCIE_READ_THROUGHPUT] =
        std::make_shared<StatsDataHandler>(MeasurementType::METRIC_PCIE_READ_THROUGHPUT, p_persistency);
    data_handlers[MeasurementType::METRIC_PCIE_READ_THROUGHPUT]->init();

    data_handlers[MeasurementType::METRIC_PCIE_WRITE_THROUGHPUT] =
        std::make_shared<StatsDataHandler>(MeasurementType::METRIC_PCIE_WRITE_THROUGHPUT, p_persistency);
    data_handlers[MeasurementType::METRIC_PCIE_WRITE_THROUGHPUT]->init();

    data_handlers[MeasurementType::METRIC_PCIE_READ] =
        std::make_shared<StatsDataHandler>(MeasurementType::METRIC_PCIE_READ, p_persistency);
    data_handlers[MeasurementType::METRIC_PCIE_READ]->init();

    data_handlers[MeasurementType::METRIC_PCIE_WRITE] =
        std::make_shared<StatsDataHandler>(MeasurementType::METRIC_PCIE_WRITE, p_persistency);
    data_handlers[MeasurementType::METRIC_PCIE_WRITE]->init();

    data_handlers[MeasurementType::METRIC_FABRIC_THROUGHPUT] =
        std::make_shared<FabricThroughputDataHandler>(MeasurementType::METRIC_FABRIC_THROUGHPUT, p_persistency);
    data_handlers[MeasurementType::METRIC_FABRIC_THROUGHPUT]->init();

    data_handlers[MeasurementType::METRIC_PERF] =
        std::make_shared<PerfMetricsHandler>(MeasurementType::METRIC_PERF, p_persistency);
    data_handlers[MeasurementType::METRIC_PERF]->init();
}

void DataHandlerManager::close() {
}

void DataHandlerManager::storeMeasurementData(
    MeasurementType type,
    Timestamp_t time,
    std::shared_ptr<std::map<std::string, std::shared_ptr<MeasurementData>>> datas) {
    std::unique_lock<std::mutex> lock(mutex);
    auto& p_handler = data_handlers[type];
    lock.unlock();

    if (p_handler != nullptr) {
        auto p_shared_data = std::make_shared<SharedData>(time, datas);
        p_handler->preHandleData(p_shared_data);
        p_handler->handleData(p_shared_data);
    }
}

std::shared_ptr<MeasurementData> DataHandlerManager::getLatestData(
    MeasurementType type,
    std::string& device_id) noexcept {
    std::unique_lock<std::mutex> lock(mutex);
    auto& p_handler = data_handlers[type];
    lock.unlock();

    return p_handler == nullptr ? nullptr : p_handler->getLatestData(device_id);
}

std::shared_ptr<MeasurementData> DataHandlerManager::getLatestStatistics(MeasurementType type, std::string& device_id, uint64_t session_id) noexcept {
    std::unique_lock<std::mutex> lock(mutex);
    auto& p_handler = data_handlers[type];
    lock.unlock();

    return p_handler == nullptr ? nullptr : p_handler->getLatestStatistics(device_id, session_id);
}

void DataHandlerManager::updateStatsTimestamp(uint32_t session_id, uint32_t device_id) {
    std::unique_lock<std::mutex> lock(mutex);
    stats_session_timestamps[session_id][device_id] = Utility::getCurrentTime();
}

uint64_t DataHandlerManager::getStatsTimestamp(uint32_t session_id, uint32_t device_id) {
    std::unique_lock<std::mutex> lock(mutex);
    uint64_t time = stats_session_timestamps[session_id][device_id];
    stats_session_timestamps[session_id][device_id] = Utility::getCurrentTime();
    return time;
}

void DataHandlerManager::updateEngineStatsTimestamp(uint32_t session_id, uint32_t device_id) {
    std::unique_lock<std::mutex> lock(mutex);
    engine_stats_session_timestamps[session_id][device_id] = Utility::getCurrentTime();
}

uint64_t DataHandlerManager::getEngineStatsTimestamp(uint32_t session_id, uint32_t device_id) {
    std::unique_lock<std::mutex> lock(mutex);
    uint64_t time = engine_stats_session_timestamps[session_id][device_id];
    engine_stats_session_timestamps[session_id][device_id] = Utility::getCurrentTime();
    return time;
}

void DataHandlerManager::updateFabricStatsTimestamp(uint32_t session_id, uint32_t device_id) {
    std::unique_lock<std::mutex> lock(mutex);
    fabric_stats_session_timestamps[session_id][device_id] = Utility::getCurrentTime();
}

uint64_t DataHandlerManager::getFabricStatsTimestamp(uint32_t session_id, uint32_t device_id) {
    std::unique_lock<std::mutex> lock(mutex);
    uint64_t time = fabric_stats_session_timestamps[session_id][device_id];
    fabric_stats_session_timestamps[session_id][device_id] = Utility::getCurrentTime();
    return time;
}

} // end namespace xpum
