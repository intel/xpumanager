#include "dump_manager.h"

namespace xpum {

xpum_result_t DumpRawDataManager::
    startDumpRawDataTask(xpum_device_id_t deviceId,
                         xpum_device_tile_id_t tileId,
                         const xpum_stats_type_t metricsTypeList[],
                         const int count,
                         const char *dumpFilePath,
                         xpum_dump_raw_data_task_t *taskInfo) {
    DumpRawDataTask task;
    task.deviceId = deviceId;
    task.tileId = tileId;
    for (int i = 0; i < count; i++) {
        task.metricsTypeList.push_back(metricsTypeList[i]);
    }
    task.begin = time(nullptr) * 1000;
    task.dumpFilePath = std::string(dumpFilePath);
    task.taskId = taskIndex++;
    taskList.push_back(task);

    // fill output buffer
    taskInfo->beginTime = task.begin;
    taskInfo->taskId = task.taskId;
    auto size = task.dumpFilePath.copy(taskInfo->dumpFilePath, task.dumpFilePath.size());
    taskInfo->dumpFilePath[size] = '\0';
    for (std::size_t i = 0; i < task.metricsTypeList.size(); i++) {
        auto buf = taskInfo->metricsTypeList;
        buf[i] = task.metricsTypeList[i];
    }
    return XPUM_OK;
}

xpum_result_t DumpRawDataManager::
    stopDumpRawDataTask(xpum_dump_task_id_t taskId, xpum_dump_raw_data_task_t *taskInfo) {
    return XPUM_GENERIC_ERROR;
}

xpum_result_t DumpRawDataManager::
    listDumpRawDataTasks(xpum_dump_raw_data_task_t taskList[], int *count) {
    return XPUM_GENERIC_ERROR;
}
} // namespace xpum