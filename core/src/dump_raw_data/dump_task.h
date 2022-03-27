/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file dump_task.h
 */

#pragma once

#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "data_logic/data_logic_interface.h"
#include "infrastructure/scheduled_thread_pool.h"
#include "xpum_structs.h"

namespace xpum {

typedef std::function<std::string()> getValueFunc;

struct DumpColumn {
    std::string header;
    getValueFunc getValue;
    uint64_t lastTimestamp = 0;

    DumpColumn(
        std::string header,
        getValueFunc getValue)
        : header(header),
          getValue(getValue) {}
};

class DumpRawDataTask : public std::enable_shared_from_this<DumpRawDataTask> {
   public:
    xpum_dump_task_id_t taskId;
    xpum_device_id_t deviceId;
    xpum_device_tile_id_t tileId;
    // std::vector<xpum_stats_type_t> metricsTypeList;
    std::vector<xpum_dump_type_t> dumpTypeList;
    std::string dumpFilePath;
    uint64_t begin;

   private:
    std::shared_ptr<ScheduledThreadPool> pThreadPool;
    std::shared_ptr<ScheduledThreadPoolTask> pThreadPoolTask;
    std::function<void()> lambda;

    std::shared_ptr<xpum::DataLogicInterface> p_data_logic;

    std::vector<DumpColumn> columnList;

    std::map<xpum_stats_type_t, xpum_device_metric_data_t> rawDataMap;
    std::map<xpum_engine_type_t, std::map<int, xpum_device_engine_metric_t>> engineUtilRawDataMap;
    std::map<std::string, xpum_device_fabric_throughput_metric_t> fabricRawDataMap;

   public:
    DumpRawDataTask(xpum_dump_task_id_t taskId,
                    xpum_device_id_t deviceId,
                    xpum_device_tile_id_t tileId,
                    std::string dumpFilePath,
                    std::shared_ptr<ScheduledThreadPool> pThreadPool);

    ~DumpRawDataTask();

    void start();

    void stop();

    void reschedule();

    void fillTaskInfoBuffer(xpum_dump_raw_data_task_t *taskInfo);

    void writeToFile(std::string text);

    void writeHeader();

    void buildColumns();

    void updateData();
};
} // namespace xpum