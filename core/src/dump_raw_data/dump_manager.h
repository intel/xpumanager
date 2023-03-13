/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file dump_manager.h
 */

#pragma once

#include <list>
#include <memory>
#include <mutex>
#include <string>

#include "dump_task.h"
#include "infrastructure/scheduled_thread_pool.h"
#include "xpum_structs.h"

namespace xpum {
class DumpRawDataManager {
    std::mutex dumpMutex; // lock for taskIndex, taskList

    int taskIndex{0};

    std::list<std::shared_ptr<DumpRawDataTask>> taskList;

    std::shared_ptr<ScheduledThreadPool> pThreadPool;

   public:
    DumpRawDataManager();

    ~DumpRawDataManager();

    void init();

    void resetDumpFrequency();

    xpum_result_t startDumpRawDataTask(xpum_device_id_t deviceId,
                                       xpum_device_tile_id_t tileId,
                                       const xpum_dump_type_t dumpTypeList[],
                                       const int count,
                                       const char *dumpFilePath,
                                       xpum_dump_raw_data_task_t *taskInfo);

    xpum_result_t stopDumpRawDataTask(xpum_dump_task_id_t taskId, xpum_dump_raw_data_task_t *taskInfo);

    xpum_result_t listDumpRawDataTasks(xpum_dump_raw_data_task_t taskList[], int *count);
};
} // namespace xpum