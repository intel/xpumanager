/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file data_logic_query_interface.h
 */

#pragma once

#include <deque>
#include <map>

#include "../include/xpum_structs.h"
#include "api/internal_api_structs.h"
#include "infrastructure/measurement_cache_data.h"
#include "infrastructure/measurement_data.h"

namespace xpum {

class DataLogicQueryInterface {
   public:
    virtual ~DataLogicQueryInterface(){};

    virtual std::shared_ptr<MeasurementData> getLatestData(MeasurementType type,
                                                           std::string &device_id) = 0;

    virtual void getLatestData(MeasurementType type,
                               std::map<std::string, std::shared_ptr<MeasurementData>> &datas) = 0;

    virtual std::shared_ptr<MeasurementData> getLatestStatistics(MeasurementType type, std::string &device_id, uint64_t session_id) = 0;

    virtual xpum_result_t getMetricsStatistics(xpum_device_id_t deviceId,
                                               xpum_device_stats_t dataList[],
                                               uint32_t *count,
                                               uint64_t *begin,
                                               uint64_t *end,
                                               uint64_t session_id) = 0;

    virtual xpum_result_t getEngineStatistics(xpum_device_id_t deviceId,
                                              xpum_device_engine_stats_t dataList[],
                                              uint32_t *count,
                                              uint64_t *begin,
                                              uint64_t *end,
                                              uint64_t session_id) = 0;

    virtual void getLatestMetrics(xpum_device_id_t deviceId,
                                  xpum_device_metrics_t dataList[],
                                  int *count) = 0;

    virtual xpum_result_t getEngineUtilizations(xpum_device_id_t deviceId,
                                                xpum_device_engine_metric_t dataList[],
                                                uint32_t *count) = 0;

    virtual xpum_result_t getFabricThroughputStatistics(xpum_device_id_t deviceId,
                                                        xpum_device_fabric_throughput_stats_t dataList[],
                                                        uint32_t *count,
                                                        uint64_t *begin,
                                                        uint64_t *end,
                                                        uint64_t session_id) = 0;

    virtual xpum_result_t getFabricThroughput(xpum_device_id_t deviceId,
                                              xpum_device_fabric_throughput_metric_t dataList[],
                                              uint32_t *count) = 0;

    virtual bool getFabricLinkInfo(xpum_device_id_t deviceId,
                                   FabricLinkInfo info[],
                                   uint32_t *count) = 0;

    virtual uint32_t startRawDataCollectionTask(xpum_device_id_t device_id, std::vector<MeasurementType> types) = 0;

    virtual void stopRawDataCollectionTask(uint32_t task_id) = 0;

    virtual std::deque<MeasurementCacheData> getCachedRawData(uint32_t task_id, MeasurementType type) = 0;

    virtual std::vector<std::deque<MeasurementCacheData>> getCachedRawData(uint32_t task_id) = 0;

    virtual void updateStatsTimestamp(uint32_t session_id, uint32_t device_id) = 0;

    virtual uint64_t getStatsTimestamp(uint32_t session_id, uint32_t device_id) = 0;

    virtual void updateEngineStatsTimestamp(uint32_t session_id, uint32_t device_id) = 0;

    virtual uint64_t getEngineStatsTimestamp(uint32_t session_id, uint32_t device_id) = 0;

    virtual void updateFabricStatsTimestamp(uint32_t session_id, uint32_t device_id) = 0;

    virtual uint64_t getFabricStatsTimestamp(uint32_t session_id, uint32_t device_id) = 0;
};
} // end namespace xpum
