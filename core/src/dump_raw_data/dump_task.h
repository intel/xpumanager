#pragma once

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "infrastructure/scheduled_thread_pool.h"
#include "xpum_structs.h"

namespace xpum {
class DumpRawDataTask : public std::enable_shared_from_this<DumpRawDataTask> {
   public:
    xpum_dump_task_id_t taskId;
    xpum_device_id_t deviceId;
    xpum_device_tile_id_t tileId;
    std::vector<xpum_stats_type_t> metricsTypeList;
    std::string dumpFilePath;
    uint64_t begin;

   private:
    std::ofstream outfile;
    std::shared_ptr<ScheduledThreadPool>& pThreadPool;
    std::shared_ptr<ScheduledThreadPoolTask> pThreadPoolTask;
    std::function<void()> lambda;

   public:
    DumpRawDataTask(xpum_dump_task_id_t taskId,
                    xpum_device_id_t deviceId,
                    xpum_device_tile_id_t tileId,
                    std::string dumpFilePath,
                    std::shared_ptr<ScheduledThreadPool>& pThreadPool);

    ~DumpRawDataTask();

    void start();

    void stop();

    void reschedule();

    void fillTaskInfoBuffer(xpum_dump_raw_data_task_t *taskInfo);

};
} // namespace xpum