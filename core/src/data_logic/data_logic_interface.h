/* 
 *  Copyright (C) 2021-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file data_logic_interface.h
 */

#pragma once

#include <map>
#include <deque>

#include "infrastructure/const.h"
#include "infrastructure/measurement_data.h"
#include "infrastructure/measurement_type.h"
#include "infrastructure/init_close_interface.h"
#include "../include/xpum_structs.h"
#include "api/internal_api_structs.h"

namespace xpum {

class DataLogicInterface : public InitCloseInterface {
    public:
        virtual ~DataLogicInterface(){};
        virtual void storeMeasurementData(
                MeasurementType type,
                Timestamp_t time,
                std::shared_ptr<std::map<std::string, std::shared_ptr<MeasurementData>>> datas) = 0;
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
        virtual void updateStatsTimestamp(uint32_t session_id, uint32_t device_id) = 0;
        virtual uint64_t getStatsTimestamp(uint32_t session_id, uint32_t device_id) = 0;
        virtual void updateEngineStatsTimestamp(uint32_t session_id, uint32_t device_id) = 0;
        virtual uint64_t getEngineStatsTimestamp(uint32_t session_id, uint32_t device_id) = 0;
        virtual void updateFabricStatsTimestamp(uint32_t session_id, uint32_t device_id) = 0;
        virtual uint64_t getFabricStatsTimestamp(uint32_t session_id, uint32_t device_id) = 0;
};

} // end namespace xpum
