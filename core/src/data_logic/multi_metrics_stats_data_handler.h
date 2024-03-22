/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file multi_metrics_stats_data_handler.h
 */

#pragma once

#include "data_handler.h"

namespace xpum {

struct Statistics_data_t {
    uint64_t count;
    uint64_t avg;
    uint64_t min;
    uint64_t max;
    long long start_time;
    long long latest_time;
    Statistics_data_t(uint64_t data, long long time) {
        min = data;
        max = data;
        avg = data;
        count = 1;
        start_time = time;
        latest_time = time;
    }
    Statistics_data_t() {
        avg = min = max = -1;
        count = start_time = latest_time = 0;
    }
};

//The map index is engine handle or fabric ID
typedef std::map<uint64_t, Statistics_data_t> multi_metrics_data_t;
//The map index is device ID
typedef std::map<std::string, multi_metrics_data_t> multi_devices_data_t;

class MultiMetricsStatsDataHandler : public DataHandler {
   public:
    MultiMetricsStatsDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~MultiMetricsStatsDataHandler();

    virtual void handleData(std::shared_ptr<SharedData> &p_data) noexcept;

    virtual std::shared_ptr<MeasurementData> getLatestData(std::string &device_id) noexcept;

    virtual std::shared_ptr<MeasurementData> getLatestStatistics(std::string &device_id, uint64_t session_id) noexcept;

   protected:
    void resetStatistics(std::string &device_id, uint64_t session_id);

    void updateStatistics(std::shared_ptr<SharedData> &p_data);

    std::map<uint64_t, multi_devices_data_t> multi_sessions_data;
};
} // end namespace xpum
