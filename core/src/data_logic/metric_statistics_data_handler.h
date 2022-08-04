/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file metric_statistics_data_handler.h
 */

#pragma once

#include "data_handler.h"

namespace xpum {

struct Statistics_subdevice_data {
    uint64_t count;
    uint64_t avg;
    uint64_t min;
    uint64_t max;
    Statistics_subdevice_data(uint64_t data) {
        min = data;
        max = data;
        avg = data;
        count = 1;
    }
};

struct Statistics_data {
    uint64_t count;
    uint64_t avg;
    uint64_t min;
    uint64_t max;
    long long start_time;
    long long latest_time;
    bool hasDataOnDevice;
    std::map<uint32_t, Statistics_subdevice_data> subdevice_datas;
    Statistics_data(uint64_t data, long long time) {
        min = data;
        max = data;
        avg = data;
        count = 1;
        start_time = time;
        latest_time = time;
        hasDataOnDevice = true;
    }
    Statistics_data(uint32_t subdevice_id, uint64_t data, long long time) {
        subdevice_datas.insert(std::make_pair(subdevice_id, Statistics_subdevice_data(data)));
        start_time = time;
        latest_time = time;
    }
};

class MetricStatisticsDataHandler : public DataHandler {
   public:
    MetricStatisticsDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~MetricStatisticsDataHandler();

    virtual void handleData(std::shared_ptr<SharedData> &p_data) noexcept;

    virtual std::shared_ptr<MeasurementData> getLatestData(std::string &device_id) noexcept;

    virtual std::shared_ptr<MeasurementData> getLatestStatistics(std::string &device_id, uint64_t session_id) noexcept;

   protected:
    void resetStatistics(std::string &device_id, uint64_t session_id);

    void updateStatistics(std::shared_ptr<SharedData> &p_data);

    std::map<uint64_t, std::map<std::string, Statistics_data>> statistics_datas;
};
} // end namespace xpum
