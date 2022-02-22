#pragma once

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "infrastructure/scheduled_thread_pool.h"
#include "xpum_structs.h"

namespace xpum {

struct DumpMetricConfigEntry{
    xpum_stats_type_t metricsType;
    std::string columnName;
};

static DumpMetricConfigEntry dumpMetricsConfigList[]{
    {XPUM_STATS_GPU_UTILIZATION, "GPU Utilization (%)"},
    {XPUM_STATS_POWER, "GPU Power (W)"},
    {XPUM_STATS_GPU_FREQUENCY, "GPU Frequency (MHz)"},
    {XPUM_STATS_GPU_CORE_TEMPERATURE, "GPU Core Temperature (Celsius Degree)"},
    {XPUM_STATS_MEMORY_TEMPERATURE, "GPU Memory Temperature (Celsius Degree)"},
    {XPUM_STATS_MEMORY_UTILIZATION, "GPU Memory Utilization (%)"},
    {XPUM_STATS_MEMORY_READ_THROUGHPUT, "GPU Memory Read (kB/s)"},
    {XPUM_STATS_MEMORY_WRITE_THROUGHPUT, "GPU Memory Write (kB/s)"},
    {XPUM_STATS_ENERGY, "GPU Energy Consumed (J)"},
    {XPUM_STATS_EU_ACTIVE, "GPU EU Array Active (%)"},
    {XPUM_STATS_EU_STALL, "GPU EU Array Stall (%)"},
    {XPUM_STATS_EU_IDLE, "GPU EU Array Idle (%)"},
    {XPUM_STATS_RAS_ERROR_CAT_RESET, "Reset Counter"},
    {XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS, "Programming Errors"},
    {XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS, "Driver Errors"},
    {XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE, "Cache Erros Correctable"},
    {XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE, "Cache Errors Uncorrectable"},
    {XPUM_STATS_MEMORY_BANDWIDTH, "GPU Memory Bandwidth Utilization (%)"},
    {XPUM_STATS_MEMORY_USED, "GPU Memory Used (MiB)"},
    {XPUM_STATS_PCIE_READ_THROUGHPUT, "PCIe Read (kB/s)"},
    {XPUM_STATS_PCIE_WRITE_THROUGHPUT, "PCIe Write (kB/s)"},
    {XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE, "GPU Memory Errors Correctable"},
    {XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE, "GPU Memory Errors Uncorrectable"}
};

bool supportedMetrics(xpum_stats_type_t metricsType);

class DumpRawDataTask : public std::enable_shared_from_this<DumpRawDataTask> {
   public:
    xpum_dump_task_id_t taskId;
    xpum_device_id_t deviceId;
    xpum_device_tile_id_t tileId;
    std::vector<xpum_stats_type_t> metricsTypeList;
    std::string dumpFilePath;
    uint64_t begin;

    uint64_t dataLastCachedTime[XPUM_STATS_MAX + 5];
   private:
    std::shared_ptr<ScheduledThreadPool> pThreadPool;
    std::shared_ptr<ScheduledThreadPoolTask> pThreadPoolTask;
    std::function<void()> lambda;

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

};
} // namespace xpum