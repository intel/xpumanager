/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file raw_data_manager.h
 */

#pragma once

#include <map>
#include <mutex>

#include "data_handler.h"
#include "infrastructure/measurement_cache_data.h"
#include "infrastructure/measurement_type.h"
#include "persistency.h"

namespace xpum {

struct RawDataCollectionTask {
    std::string device_id;
    std::vector<MeasurementType> types;
    uint32_t task_id;
    bool running;
    uint64_t stop_time;
    uint64_t start_time;
    std::map<MeasurementType, uint32_t> time_frames_count;
    RawDataCollectionTask(std::string& device_id, std::vector<MeasurementType>& types, uint32_t task_id)
        : device_id(device_id), types(types), task_id(task_id), running(true), stop_time(-1) {
        start_time = Utility::getCurrentMillisecond();
        for (auto type : types) {
            time_frames_count[type] = 0;
        }
    }
};

class RawDataManager {
   public:
    RawDataManager(std::shared_ptr<Persistency>& persistency);

    virtual ~RawDataManager();

    void init();

    void close();

    void storeMeasurementData(
        MeasurementType type,
        Timestamp_t time,
        std::shared_ptr<std::map<std::string, std::shared_ptr<MeasurementData>>> datas);

    std::shared_ptr<MeasurementData> getLatestData(
        MeasurementType type,
        std::string& device_id) noexcept;

    void getLatestData(
        MeasurementType type,
        std::map<std::string, std::shared_ptr<MeasurementData>>& datas) noexcept;

    std::shared_ptr<MeasurementData> getLatestStatistics(MeasurementType type, std::string& device_id, uint64_t session_id) noexcept;

    uint32_t startRawDataCollectionTask(std::string& device_id, std::vector<MeasurementType>& types);

    void stopRawDataCollectionTask(uint32_t task_id);

    std::deque<MeasurementCacheData> getCachedRawData(uint32_t task_id, MeasurementType type);

    std::vector<std::deque<MeasurementCacheData>> getCachedRawData(uint32_t task_id);

    void updateStatsTimestamp(uint32_t session_id, uint32_t device_id);

    uint64_t getStatsTimestamp(uint32_t session_id, uint32_t device_id);

    void updateEngineStatsTimestamp(uint32_t session_id, uint32_t device_id);

    uint64_t getEngineStatsTimestamp(uint32_t session_id, uint32_t device_id);

    void updateFabricStatsTimestamp(uint32_t session_id, uint32_t device_id);

    uint64_t getFabricStatsTimestamp(uint32_t session_id, uint32_t device_id);

   private:
    RawDataManager() = default;

    void updateCaches(MeasurementType type, std::shared_ptr<SharedData>& p_data);

   private:
    std::map<MeasurementType, std::shared_ptr<DataHandler>> data_handlers;

    std::shared_ptr<Persistency> p_persistency;

    std::map<uint32_t, std::map<MeasurementType, std::deque<MeasurementCacheData>>> caches;

    std::deque<RawDataCollectionTask> raw_data_collection_tasks;

    std::map<uint32_t, std::map<uint32_t, uint64_t>> stats_session_timestamps;

    std::map<uint32_t, std::map<uint32_t, uint64_t>> engine_stats_session_timestamps;

    std::map<uint32_t, std::map<uint32_t, uint64_t>> fabric_stats_session_timestamps;

    std::mutex mutex;
};

} // end namespace xpum
