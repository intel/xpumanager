/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file raw_data_manager.cpp
 */

#include "raw_data_manager.h"

#include <algorithm>

#include "engine_group_utilization_data_handler.h"
#include "engine_utilization_data_handler.h"
#include "fabric_throughput_data_handler.h"
#include "frequency_data_handler.h"
#include "frequency_throttle_time_data_handler.h"
#include "gpu_utilization_data_handler.h"
#include "infrastructure/configuration.h"
#include "memory_data_handler.h"
#include "metric_statistics_data_handler.h"
#include "power_data_handler.h"
#include "shared_data.h"
#include "temperature_data_handler.h"
#include "throughput_data_handler.h"
#include "perf_metrics_data_handler.h"

namespace xpum {

RawDataManager::RawDataManager(std::shared_ptr<Persistency>& persistency)
    : p_persistency(persistency) {
}

RawDataManager::~RawDataManager() {
    close();
}

void RawDataManager::init() {
    std::unique_lock<std::mutex> lock(mutex);

    data_handlers[MeasurementType::METRIC_TEMPERATURE] =
        std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_TEMPERATURE, p_persistency);
    data_handlers[MeasurementType::METRIC_TEMPERATURE]->init();

    data_handlers[MeasurementType::METRIC_FREQUENCY] =
        std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_FREQUENCY, p_persistency);
    data_handlers[MeasurementType::METRIC_FREQUENCY]->init();

    data_handlers[MeasurementType::METRIC_REQUEST_FREQUENCY] =
        std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_REQUEST_FREQUENCY, p_persistency);
    data_handlers[MeasurementType::METRIC_REQUEST_FREQUENCY]->init();

    data_handlers[MeasurementType::METRIC_POWER] =
        std::make_shared<PowerDataHandler>(MeasurementType::METRIC_POWER, p_persistency);
    data_handlers[MeasurementType::METRIC_POWER]->init();

    data_handlers[MeasurementType::METRIC_ENERGY] =
        std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_ENERGY, p_persistency);
    data_handlers[MeasurementType::METRIC_ENERGY]->init();

    data_handlers[MeasurementType::METRIC_MEMORY_USED] =
        std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_MEMORY_USED, p_persistency);
    data_handlers[MeasurementType::METRIC_MEMORY_USED]->init();

    data_handlers[MeasurementType::METRIC_MEMORY_UTILIZATION] =
        std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_MEMORY_UTILIZATION, p_persistency);
    data_handlers[MeasurementType::METRIC_MEMORY_UTILIZATION]->init();

    data_handlers[MeasurementType::METRIC_MEMORY_BANDWIDTH] =
        std::make_shared<MemoryDataHandler>(MeasurementType::METRIC_MEMORY_BANDWIDTH, p_persistency);
    data_handlers[MeasurementType::METRIC_MEMORY_BANDWIDTH]->init();

    data_handlers[MeasurementType::METRIC_MEMORY_READ] =
        std::make_shared<MemoryDataHandler>(MeasurementType::METRIC_MEMORY_READ, p_persistency);
    data_handlers[MeasurementType::METRIC_MEMORY_READ]->init();

    data_handlers[MeasurementType::METRIC_MEMORY_WRITE] =
        std::make_shared<MemoryDataHandler>(MeasurementType::METRIC_MEMORY_WRITE, p_persistency);
    data_handlers[MeasurementType::METRIC_MEMORY_WRITE]->init();

    data_handlers[MeasurementType::METRIC_MEMORY_READ_THROUGHPUT] =
        std::make_shared<ThroughputDataHandler>(MeasurementType::METRIC_MEMORY_READ_THROUGHPUT, p_persistency);
    data_handlers[MeasurementType::METRIC_MEMORY_READ_THROUGHPUT]->init();

    data_handlers[MeasurementType::METRIC_MEMORY_WRITE_THROUGHPUT] =
        std::make_shared<ThroughputDataHandler>(MeasurementType::METRIC_MEMORY_WRITE_THROUGHPUT, p_persistency);
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
        std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_EU_ACTIVE, p_persistency);
    data_handlers[MeasurementType::METRIC_EU_ACTIVE]->init();

    data_handlers[MeasurementType::METRIC_EU_STALL] =
        std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_EU_STALL, p_persistency);
    data_handlers[MeasurementType::METRIC_EU_STALL]->init();

    data_handlers[MeasurementType::METRIC_EU_IDLE] =
        std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_EU_IDLE, p_persistency);
    data_handlers[MeasurementType::METRIC_EU_IDLE]->init();

    //METRIC_RAS_ERROR
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_RESET] = std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_RESET, p_persistency);
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_RESET]->init();
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS] = std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS, p_persistency);
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS]->init();
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS] = std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS, p_persistency);
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS]->init();
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE] = std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE, p_persistency);
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE]->init();
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE] = std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE, p_persistency);
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE]->init();
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE] = std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE, p_persistency);
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE]->init();
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE] = std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE, p_persistency);
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE]->init();
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE] = std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE, p_persistency);
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE]->init();
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE] = std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE, p_persistency);
    data_handlers[MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE]->init();

    data_handlers[MeasurementType::METRIC_MEMORY_TEMPERATURE] =
        std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_MEMORY_TEMPERATURE, p_persistency);
    data_handlers[MeasurementType::METRIC_MEMORY_TEMPERATURE]->init();

    data_handlers[MeasurementType::METRIC_FREQUENCY_THROTTLE] =
        std::make_shared<FrequencyThrottleTimeDataHandler>(MeasurementType::METRIC_FREQUENCY_THROTTLE, p_persistency);
    data_handlers[MeasurementType::METRIC_FREQUENCY_THROTTLE]->init();

    data_handlers[MeasurementType::METRIC_FREQUENCY_THROTTLE_REASON_GPU] =
        std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_FREQUENCY_THROTTLE_REASON_GPU, p_persistency);

    data_handlers[MeasurementType::METRIC_PCIE_READ_THROUGHPUT] =
        std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_PCIE_READ_THROUGHPUT, p_persistency);
    data_handlers[MeasurementType::METRIC_PCIE_READ_THROUGHPUT]->init();

    data_handlers[MeasurementType::METRIC_PCIE_WRITE_THROUGHPUT] =
        std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_PCIE_WRITE_THROUGHPUT, p_persistency);
    data_handlers[MeasurementType::METRIC_PCIE_WRITE_THROUGHPUT]->init();

    data_handlers[MeasurementType::METRIC_PCIE_READ] =
        std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_PCIE_READ, p_persistency);
    data_handlers[MeasurementType::METRIC_PCIE_READ]->init();

    data_handlers[MeasurementType::METRIC_PCIE_WRITE] =
        std::make_shared<MetricStatisticsDataHandler>(MeasurementType::METRIC_PCIE_WRITE, p_persistency);
    data_handlers[MeasurementType::METRIC_PCIE_WRITE]->init();

    data_handlers[MeasurementType::METRIC_FABRIC_THROUGHPUT] =
        std::make_shared<FabricThroughputDataHandler>(MeasurementType::METRIC_FABRIC_THROUGHPUT, p_persistency);
    data_handlers[MeasurementType::METRIC_FABRIC_THROUGHPUT]->init();

    data_handlers[MeasurementType::METRIC_PERF] =
        std::make_shared<PerfMetricsHandler>(MeasurementType::METRIC_PERF, p_persistency);
    data_handlers[MeasurementType::METRIC_PERF]->init();
}

void RawDataManager::close() {
}

void RawDataManager::storeMeasurementData(
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
        updateCaches(type, p_shared_data);
    }
}

std::shared_ptr<MeasurementData> RawDataManager::getLatestData(
    MeasurementType type,
    std::string& device_id) noexcept {
    std::unique_lock<std::mutex> lock(mutex);
    auto& p_handler = data_handlers[type];
    lock.unlock();

    return p_handler == nullptr ? nullptr : p_handler->getLatestData(device_id);
}

std::shared_ptr<MeasurementData> RawDataManager::getLatestStatistics(MeasurementType type, std::string& device_id, uint64_t session_id) noexcept {
    std::unique_lock<std::mutex> lock(mutex);
    auto& p_handler = data_handlers[type];
    lock.unlock();

    return p_handler == nullptr ? nullptr : p_handler->getLatestStatistics(device_id, session_id);
}

void RawDataManager::getLatestData(
    MeasurementType type,
    std::map<std::string, std::shared_ptr<MeasurementData>>& datas) noexcept {
    std::unique_lock<std::mutex> lock(mutex);
    auto& p_handler = data_handlers[type];
    lock.unlock();

    if (p_handler != nullptr) {
        p_handler->getLatestData(datas);
    }
}

std::deque<MeasurementCacheData> RawDataManager::getCachedRawData(uint32_t task_id, MeasurementType type) {
    std::unique_lock<std::mutex> lock(mutex);
    if (caches.find(task_id) != caches.end() && caches.find(task_id)->second.find(type) != caches.find(task_id)->second.end()) {
        return caches[task_id][type];
    }
    std::deque<MeasurementCacheData> ret;
    return ret;
}

std::vector<std::deque<MeasurementCacheData>> RawDataManager::getCachedRawData(uint32_t task_id) {
    std::unique_lock<std::mutex> lock(mutex);
    std::vector<std::deque<MeasurementCacheData>> datas;
    std::vector<MeasurementType> types;
    std::deque<RawDataCollectionTask>::iterator iter = std::find_if(raw_data_collection_tasks.begin(), raw_data_collection_tasks.end(), [task_id](RawDataCollectionTask& task) { return task.task_id == task_id && task.running == false; });
    if (iter != raw_data_collection_tasks.end()) {
        types = iter->types;
    }
    lock.unlock();
    for (auto type : types) {
        datas.push_back(getCachedRawData(task_id, type));
    }
    return datas;
}

uint32_t RawDataManager::startRawDataCollectionTask(std::string& device_id, std::vector<MeasurementType>& types) {
    std::unique_lock<std::mutex> lock(mutex);
    uint32_t task_id = Configuration::RAW_DATA_COLLECTION_TASK_NUM_MAX;
    if (raw_data_collection_tasks.size() < Configuration::RAW_DATA_COLLECTION_TASK_NUM_MAX) {
        task_id = raw_data_collection_tasks.size();
        RawDataCollectionTask task(device_id, types, task_id);
        raw_data_collection_tasks.push_back(task);
    } else {
        std::deque<RawDataCollectionTask>::iterator iter =
            std::find_if(raw_data_collection_tasks.begin(), raw_data_collection_tasks.end(), [](RawDataCollectionTask& task) { return task.running == false; });
        task_id = iter->task_id;
        if (iter != raw_data_collection_tasks.end()) {
            raw_data_collection_tasks.erase(iter);
            caches[task_id].clear();
        }
        if (raw_data_collection_tasks.size() < Configuration::RAW_DATA_COLLECTION_TASK_NUM_MAX) {
            RawDataCollectionTask task(device_id, types, task_id);
            raw_data_collection_tasks.push_back(task);
        }
    }
    return task_id;
}

void RawDataManager::stopRawDataCollectionTask(uint32_t task_id) {
    std::unique_lock<std::mutex> lock(mutex);
    std::deque<RawDataCollectionTask>::iterator iter = std::find_if(raw_data_collection_tasks.begin(), raw_data_collection_tasks.end(), [task_id](RawDataCollectionTask& task) { return task.task_id == task_id; });
    if (iter != raw_data_collection_tasks.end() && iter->running) {
        iter->running = false;
        iter->stop_time = Utility::getCurrentMillisecond();
    }
}

void RawDataManager::updateCaches(MeasurementType type, std::shared_ptr<SharedData>& p_data) {
    std::unique_lock<std::mutex> lock(mutex);
    std::deque<RawDataCollectionTask>::iterator iter = raw_data_collection_tasks.begin();
    while (iter != raw_data_collection_tasks.end()) {
        if (iter->running) {
            bool added = false;
            if (iter->time_frames_count[type] < Configuration::CACHE_SIZE_LIMIT) {
                std::map<std::string, std::shared_ptr<MeasurementData>>::iterator iter_p_data = p_data->getData().begin();
                while (iter_p_data != p_data->getData().end()) {
                    if (iter_p_data->first == iter->device_id) {
                        if (iter_p_data->second->hasDataOnDevice()) {
                            MeasurementCacheData data(iter_p_data->first, type, iter_p_data->second->getCurrent(), p_data->getTime(), false, 0);
                            caches[iter->task_id][type].push_back(data);
                            added = true;
                        }
                        std::map<uint32_t, SubdeviceData>::const_iterator iter_p_data_sub = iter_p_data->second->getSubdeviceDatas()->begin();
                        while (iter_p_data_sub != iter_p_data->second->getSubdeviceDatas()->end()) {
                            MeasurementCacheData data(iter_p_data->first, type, iter_p_data_sub->second.current, p_data->getTime(), true, iter_p_data_sub->first);
                            caches[iter->task_id][type].push_back(data);
                            added = true;
                            ++iter_p_data_sub;
                        }
                    }
                    ++iter_p_data;
                }
            } else {
                auto frames = std::min_element(iter->time_frames_count.begin(), iter->time_frames_count.end(),
                                               [](decltype(iter->time_frames_count)::value_type& l, decltype(iter->time_frames_count)::value_type& r) -> bool { return l.second < r.second; });
                if (frames->second >= Configuration::CACHE_SIZE_LIMIT) {
                    iter->running = false;
                    iter->stop_time = Utility::getCurrentMillisecond();
                }
            }
            if (added) {
                iter->time_frames_count[type]++;
            }
        }
        ++iter;
    }
}

void RawDataManager::updateStatsTimestamp(uint32_t session_id, uint32_t device_id) {
    std::unique_lock<std::mutex> lock(mutex);
    stats_session_timestamps[session_id][device_id] = Utility::getCurrentTime();
}

uint64_t RawDataManager::getStatsTimestamp(uint32_t session_id, uint32_t device_id) {
    std::unique_lock<std::mutex> lock(mutex);
    uint64_t time = stats_session_timestamps[session_id][device_id];
    stats_session_timestamps[session_id][device_id] = Utility::getCurrentTime();
    return time;
}

void RawDataManager::updateEngineStatsTimestamp(uint32_t session_id, uint32_t device_id) {
    std::unique_lock<std::mutex> lock(mutex);
    engine_stats_session_timestamps[session_id][device_id] = Utility::getCurrentTime();
}

uint64_t RawDataManager::getEngineStatsTimestamp(uint32_t session_id, uint32_t device_id) {
    std::unique_lock<std::mutex> lock(mutex);
    uint64_t time = engine_stats_session_timestamps[session_id][device_id];
    engine_stats_session_timestamps[session_id][device_id] = Utility::getCurrentTime();
    return time;
}

void RawDataManager::updateFabricStatsTimestamp(uint32_t session_id, uint32_t device_id) {
    std::unique_lock<std::mutex> lock(mutex);
    fabric_stats_session_timestamps[session_id][device_id] = Utility::getCurrentTime();
}

uint64_t RawDataManager::getFabricStatsTimestamp(uint32_t session_id, uint32_t device_id) {
    std::unique_lock<std::mutex> lock(mutex);
    uint64_t time = fabric_stats_session_timestamps[session_id][device_id];
    fabric_stats_session_timestamps[session_id][device_id] = Utility::getCurrentTime();
    return time;
}

} // end namespace xpum
