#include "dump_task.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <map>

#include "core/core.h"
#include "infrastructure/configuration.h"

namespace xpum {
DumpRawDataTask::DumpRawDataTask(xpum_dump_task_id_t taskId,
                                 xpum_device_id_t deviceId,
                                 xpum_device_tile_id_t tileId,
                                 std::string dumpFilePath,
                                 std::shared_ptr<ScheduledThreadPool> pThreadPool) : taskId(taskId),
                                                                                      deviceId(deviceId),
                                                                                      tileId(tileId),
                                                                                      dumpFilePath(dumpFilePath),
                                                                                      pThreadPool(pThreadPool) {
}

DumpRawDataTask::~DumpRawDataTask() {
    // Attention: need to stop task first, then close file
    // stop periodical task
    // pThreadPoolTask->cancel();
    stop();
    std::cout << "~DumpRawDataTask() called" << std::endl;
}

void DumpRawDataTask::writeToFile(std::string text) {
    std::ofstream outfile;
    outfile.open(dumpFilePath.c_str(), std::ios::out | std::ios::app);
    outfile << text << std::endl;
    outfile.flush();
    outfile.close();
}

static std::string translate(xpum_stats_type_t metricsType) {
    switch (metricsType) {
        case XPUM_STATS_GPU_UTILIZATION:
            return "GPU Utilization (%)";
        case XPUM_STATS_POWER:
            return "GPU Power (W)";
        case XPUM_STATS_GPU_FREQUENCY:
            return "GPU Frequency (MHz)";
        case XPUM_STATS_GPU_CORE_TEMPERATURE:
            return "GPU Core Temperature (Celsius Degree)";
        case XPUM_STATS_MEMORY_TEMPERATURE:
            return "GPU Memory Temperature (Celsius Degree)";
        case XPUM_STATS_MEMORY_UTILIZATION:
            return "GPU Memory Utilization (%)";
        case XPUM_STATS_MEMORY_READ_THROUGHPUT:
            return "GPU Memory Read (kB/s)";
        case XPUM_STATS_MEMORY_WRITE_THROUGHPUT:
            return "GPU Memory Write (kB/s)";
        case XPUM_STATS_ENERGY:
            return "GPU Energy Consumed (J)";
        case XPUM_STATS_EU_ACTIVE:
            return "GPU EU Array Active (%)";
        case XPUM_STATS_EU_STALL:
            return "GPU EU Array Stall (%)";
        case XPUM_STATS_EU_IDLE:
            return "GPU EU Array Idle (%)";
        case XPUM_STATS_RAS_ERROR_CAT_RESET:
            return "Reset Counter";
        case XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS:
            return "Programming Errors";
        case XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS:
            return "Driver Errors";
        case XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE:
            return "Cache Erros Correctable";
        case XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE:
            return "Cache Errors Uncorrectable";
        case XPUM_STATS_MEMORY_BANDWIDTH:
            return "GPU Memory Bandwidth Utilization (%)";
        case XPUM_STATS_MEMORY_USED:
            return "GPU Memory Used (MiB)";
        default:
            return "";
    }
}

void DumpRawDataTask::writeHeader() {
    std::ofstream outfile;
    outfile.open(dumpFilePath.c_str(), std::ios::out | std::ios::app);
    outfile << "Timestamp,DeviceId";
    if (tileId != -1) {
        outfile << ",TileId";
    }
    for (auto metricsType : metricsTypeList) {
        outfile << "," << translate(metricsType);
    }
    outfile<<std::endl;
    outfile.flush();
    outfile.close();
}

void DumpRawDataTask::start() {
    // start periodical task to write to output file

    begin = time(nullptr) * 1000;

    // write to file with header
    writeHeader();

    auto p_this = shared_from_this();
    lambda = [p_this]() {
        std::stringstream ss;

        // timestamp string
        ss << Utility::getCurrentUTCTimeString();

        // device id
        ss << "," << p_this->deviceId;

        // tile id
        if (p_this->tileId != -1) {
            ss << "," << p_this->tileId;
        }

        auto p_data_logic = xpum::Core::instance().getDataLogic();

        int metricsCount;
        p_data_logic->getLatestMetrics(p_this->deviceId,nullptr, &metricsCount);
        std::vector<xpum_device_metrics_t> deviceMetricsList(metricsCount);
        p_data_logic->getLatestMetrics(p_this->deviceId, deviceMetricsList.data(), &metricsCount);

        std::map<xpum_stats_type_t,xpum_device_metric_data_t> m;
        
        for (auto deviceMetrics : deviceMetricsList) {
            if ((p_this->tileId == -1 && !deviceMetrics.isTileData) || (deviceMetrics.isTileData && (p_this->tileId == deviceMetrics.tileId))) {
                for (int i = 0; i < deviceMetrics.count; i++) {
                    auto data = deviceMetrics.dataList[i];
                    m[data.metricsType] = data;
                }
                break;
            }
        }

        for (auto metricsType : p_this->metricsTypeList) {
            auto it = m.find(metricsType);
            if (it == m.end()) {
                ss << ",";
                continue;
            }
            auto data = it->second;
            auto t = data.timestamp;
            if (p_this->dataLastCachedTime[metricsType] != t) {
                p_this->dataLastCachedTime[metricsType] = t;
                uint64_t scale = data.scale;
                uint64_t rawData = data.value;
                // some metrics need unit transform
                switch (metricsType) {
                    case XPUM_STATS_ENERGY:
                        rawData /= 1000;
                        break;
                    case XPUM_STATS_MEMORY_USED:
                        rawData /= (1024 * 1024);
                        break;
                    default:
                        break;
                }
                if (scale == 1) {
                    ss << "," << rawData;
                } else {
                    double d = rawData / scale;
                    ss << "," << d;
                }
            } else {
                ss << ",";
            }
        }
        p_this->writeToFile(ss.str());
    };
    // schedule task
    pThreadPoolTask = pThreadPool->scheduleAtFixedRate(0, Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, lambda);
}

void DumpRawDataTask::stop() {
    if (pThreadPoolTask != nullptr) {
        pThreadPoolTask->cancel();
        pThreadPoolTask.reset();
    }
}

void DumpRawDataTask::reschedule() {
    // stop task first
    stop();
    // reschedule the task to refresh the dump interval
    pThreadPoolTask = pThreadPool->scheduleAtFixedRate(0, Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, lambda);
}

void DumpRawDataTask::fillTaskInfoBuffer(xpum_dump_raw_data_task_t* taskInfo) {
    taskInfo->beginTime = begin;
    taskInfo->taskId = taskId;
    auto size = dumpFilePath.copy(taskInfo->dumpFilePath, dumpFilePath.size());
    taskInfo->dumpFilePath[size] = '\0';
    for (std::size_t i = 0; i < metricsTypeList.size(); i++) {
        auto buf = taskInfo->metricsTypeList;
        buf[i] = metricsTypeList[i];
    }
    taskInfo->count = metricsTypeList.size();
}

} // namespace xpum