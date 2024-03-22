/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file raw_data_manager.h
 */

#pragma once

#include <map>
#include <mutex>

#include "data_handler.h"
#include "infrastructure/measurement_type.h"
#include "persistency.h"

namespace xpum {


class DataHandlerManager {
    public:
    DataHandlerManager(std::shared_ptr<Persistency>& persistency);

    virtual ~DataHandlerManager();

    void init();

    void close();

    void storeMeasurementData(
        MeasurementType type,
        Timestamp_t time,
        std::shared_ptr<std::map<std::string, std::shared_ptr<MeasurementData>>> datas);

    std::shared_ptr<MeasurementData> getLatestData(
        MeasurementType type,
        std::string& device_id) noexcept;

    std::shared_ptr<MeasurementData> getLatestStatistics(MeasurementType type, std::string& device_id, uint64_t session_id) noexcept;

    void updateStatsTimestamp(uint32_t session_id, uint32_t device_id);

    uint64_t getStatsTimestamp(uint32_t session_id, uint32_t device_id);

    void updateEngineStatsTimestamp(uint32_t session_id, uint32_t device_id);

    uint64_t getEngineStatsTimestamp(uint32_t session_id, uint32_t device_id);

    void updateFabricStatsTimestamp(uint32_t session_id, uint32_t device_id);

    uint64_t getFabricStatsTimestamp(uint32_t session_id, uint32_t device_id);

    private:
    DataHandlerManager() = default;

    std::map<MeasurementType, std::shared_ptr<DataHandler>> data_handlers;

    std::shared_ptr<Persistency> p_persistency;

    std::map<uint32_t, std::map<uint32_t, uint64_t>> stats_session_timestamps;

    std::map<uint32_t, std::map<uint32_t, uint64_t>> engine_stats_session_timestamps;

    std::map<uint32_t, std::map<uint32_t, uint64_t>> fabric_stats_session_timestamps;

    std::mutex mutex;
};

} // end namespace xpum
