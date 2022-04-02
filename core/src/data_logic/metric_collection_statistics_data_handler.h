/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file metric_collection_statistics_data_handler.h
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

class MetricCollectionStatisticsDataHandler : public DataHandler {
   public:
    MetricCollectionStatisticsDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~MetricCollectionStatisticsDataHandler();

    virtual void handleData(std::shared_ptr<SharedData> &p_data) noexcept;

    virtual std::shared_ptr<MeasurementData> getLatestData(std::string &device_id) noexcept;

    virtual std::shared_ptr<MeasurementData> getLatestStatistics(std::string &device_id, uint64_t session_id) noexcept;

   protected:
    void resetStatistics(std::string &device_id, uint64_t session_id);

    void updateStatistics(std::shared_ptr<SharedData> &p_data);

    std::map<uint64_t, std::map<std::string, std::map<uint64_t, Statistics_data_t>>> statistics_datas;
};
} // end namespace xpum
