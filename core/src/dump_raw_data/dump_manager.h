#include <atomic>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "control/device_manager_interface.h"
#include "data_logic/data_logic_interface.h"
#include "xpum_structs.h"

namespace xpum {
struct DumpRawDataTask {
    xpum_dump_task_id_t taskId;
    xpum_device_id_t deviceId;
    xpum_device_tile_id_t tileId;
    std::vector<xpum_stats_type_t> metricsTypeList;
    std::string dumpFilePath;
    uint64_t begin;
};

class DumpRawDataManager {
    // change to mutex
    std::atomic<xpum_dump_task_id_t> taskIndex{0};

    std::list<DumpRawDataTask> taskList;

   public:
    DumpRawDataManager() {
    }

    void init() {
    }

    xpum_result_t startDumpRawDataTask(xpum_device_id_t deviceId,
                                       xpum_device_tile_id_t tileId,
                                       const xpum_stats_type_t metricsTypeList[],
                                       const int count,
                                       const char *dumpFilePath,
                                       xpum_dump_raw_data_task_t *taskInfo);
    xpum_result_t stopDumpRawDataTask(xpum_dump_task_id_t taskId, xpum_dump_raw_data_task_t *taskInfo);
    xpum_result_t listDumpRawDataTasks(xpum_dump_raw_data_task_t taskList[], int *count);
};
} // namespace xpum