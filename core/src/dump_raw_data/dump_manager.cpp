#include "dump_manager.h"
#include "xpum_structs.h"

#include <iostream>
#include <mutex>

#include "core/core.h"

#include "dump_task.h"
#include "api/internal_dump_raw_data.h"

using xpum::dump::getConfigOptionPointer;

namespace xpum {

DumpRawDataManager::DumpRawDataManager() {
    pThreadPool = std::make_shared<ScheduledThreadPool>(2);
}

DumpRawDataManager::~DumpRawDataManager() {
    pThreadPool->close();
    // std::cout << "DumpRawDataManager::~DumpRawDataManager() called" << std::endl;
}

void DumpRawDataManager::resetDumpFrequency() {
    std::lock_guard<std::mutex> lock(dumpMutex);
    for (auto task : taskList) {
        task->reschedule();
    }
}

xpum_result_t DumpRawDataManager::
    startDumpRawDataTask(xpum_device_id_t deviceId,
                         xpum_device_tile_id_t tileId,
                         const xpum_dump_type_t dumpTypeList[],
                         const int count,
                         const char *dumpFilePath,
                         xpum_dump_raw_data_task_t *taskInfo) {
    std::lock_guard<std::mutex> lock(dumpMutex);

    for (int i = 0; i < count; i++) {
        auto dumpType = dumpTypeList[i];
        if (getConfigOptionPointer(dumpType) == nullptr)
            return XPUM_RESULT_DUMP_METRICS_TYPE_NOT_SUPPORT;
    }

    // create task
    std::shared_ptr<DumpRawDataTask> p_task = std::make_shared<DumpRawDataTask>(taskIndex++, deviceId, tileId, std::string(dumpFilePath), pThreadPool);
    for (int i = 0; i < count; i++) {
        // p_task->metricsTypeList.push_back(dumpTypeList[i]);
        p_task->dumpTypeList.push_back(dumpTypeList[i]);
    }
    taskList.push_back(p_task);

    // start p_task
    p_task->start();

    // fill output buffer
    p_task->fillTaskInfoBuffer(taskInfo);
    return XPUM_OK;
}

xpum_result_t DumpRawDataManager::
    stopDumpRawDataTask(xpum_dump_task_id_t taskId, xpum_dump_raw_data_task_t *taskInfo) {
    std::lock_guard<std::mutex> lock(dumpMutex);
    auto iter = taskList.begin();
    while (iter != taskList.end()) {
        auto p_task = *iter;
        if (p_task->taskId == taskId) {
            //found
            p_task->stop();
            p_task->fillTaskInfoBuffer(taskInfo);
            taskList.erase(iter);
            return XPUM_OK;
        }
        iter++;
    }
    return XPUM_DUMP_RAW_DATA_TASK_NOT_EXIST;
}

xpum_result_t DumpRawDataManager::
    listDumpRawDataTasks(xpum_dump_raw_data_task_t taskInfoList[], int *count) {
    std::lock_guard<std::mutex> lock(dumpMutex);
    if (taskInfoList == nullptr) {
        *count = (int)taskList.size();
        return XPUM_OK;
    } else {
        if (*count < (int)taskList.size())
            return XPUM_BUFFER_TOO_SMALL;
    }
    auto iter = taskList.begin();
    int i = 0;
    while (iter != taskList.end()) {
        auto p_task = *iter;
        p_task->fillTaskInfoBuffer(taskInfoList + i);
        iter++;
        i++;
    }
    *count = i;
    return XPUM_OK;
}

void DumpRawDataManager::init() {
}
} // namespace xpum