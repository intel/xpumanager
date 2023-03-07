/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file data_logic.h
 */

#pragma once

#include "../include/xpum_structs.h"
#include "data_logic_interface.h"
#include "infrastructure/const.h"
#include "persistency.h"
#include "raw_data_manager.h"

namespace xpum {

/*
  DataLogic provides various interfaces to obtain various monitoring data.
*/

class DataLogic : public DataLogicInterface {
   public:
    DataLogic();

    virtual ~DataLogic();

    void init() override;

    void close() override;

    void storeMeasurementData(
        MeasurementType type,
        Timestamp_t time,
        std::shared_ptr<std::map<std::string, std::shared_ptr<MeasurementData>>> datas) override;

    std::shared_ptr<MeasurementData> getLatestData(
        MeasurementType type,
        std::string& device_id) override;

    void getLatestData(
        MeasurementType type,
        std::map<std::string, std::shared_ptr<MeasurementData>>& datas) override;

    std::shared_ptr<MeasurementData> getLatestStatistics(MeasurementType type, std::string& device_id, uint64_t session_id) override;

    xpum_result_t getMetricsStatistics(xpum_device_id_t device_id,
                                       xpum_device_stats_t data_list[],
                                       uint32_t* count,
                                       uint64_t* begin,
                                       uint64_t* end,
                                       uint64_t session_id);

    xpum_result_t getEngineStatistics(xpum_device_id_t device_id,
                                      xpum_device_engine_stats_t data_list[],
                                      uint32_t* count,
                                      uint64_t* begin,
                                      uint64_t* end,
                                      uint64_t session_id);

    xpum_result_t getFabricThroughputStatistics(xpum_device_id_t deviceId,
                                                xpum_device_fabric_throughput_stats_t dataList[],
                                                uint32_t* count,
                                                uint64_t* begin,
                                                uint64_t* end,
                                                uint64_t session_id);

    xpum_result_t getFabricThroughput(xpum_device_id_t deviceId,
                                      xpum_device_fabric_throughput_metric_t dataList[],
                                      uint32_t* count);

    bool getFabricLinkInfo(xpum_device_id_t deviceId,
                           FabricLinkInfo info[],
                           uint32_t* count);

    void getLatestMetrics(xpum_device_id_t deviceId,
                          xpum_device_metrics_t dataList[],
                          int* count);

    xpum_result_t getEngineUtilizations(xpum_device_id_t deviceId,
                                        xpum_device_engine_metric_t dataList[],
                                        uint32_t* count);

    uint32_t startRawDataCollectionTask(xpum_device_id_t device_id, std::vector<MeasurementType> types);

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
    std::unique_ptr<RawDataManager> p_raw_data_manager;

    std::shared_ptr<Persistency> p_persistency;
};

} // end namespace xpum
