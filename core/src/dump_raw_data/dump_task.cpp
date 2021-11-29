#include "dump_task.h"

#include <fstream>
#include <iostream>

#include "core/core.h"
#include "infrastructure/configuration.h"

namespace xpum {
DumpRawDataTask::DumpRawDataTask(xpum_dump_task_id_t taskId,
                                 xpum_device_id_t deviceId,
                                 xpum_device_tile_id_t tileId,
                                 std::string dumpFilePath,
                                 std::shared_ptr<ScheduledThreadPool>& pThreadPool) : taskId(taskId),
                                                                                      deviceId(deviceId),
                                                                                      tileId(tileId),
                                                                                      dumpFilePath(dumpFilePath),
                                                                                      outfile(dumpFilePath, std::ofstream::binary),
                                                                                      pThreadPool(pThreadPool) {
    begin = time(nullptr) * 1000;
    // write to file with header
    outfile << "this is header" << std::endl;
    // start periodical task to write to output file
    auto p_data_logic = xpum::Core::instance().getDataLogic();
    auto p_this = shared_from_this();
    lambda = [p_this]() {
        p_this->outfile << "hello" << std::endl;
    };
}

DumpRawDataTask::~DumpRawDataTask() {
    std::cout << "~DumpRawDataTask() called" << std::endl;
    // Attention: need to stop task first, then close file
    // stop periodical task
    pThreadPoolTask->cancel();
    // close file
    outfile.close();
}

void DumpRawDataTask::start() {
    // schedule task
    pThreadPoolTask = pThreadPool->scheduleAtFixedRate(0, Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, lambda);
}

void DumpRawDataTask::stop() {
    if (!pThreadPoolTask) {
        pThreadPoolTask->cancel();
        pThreadPoolTask.reset();
    }
}

void DumpRawDataTask::reschedule(int period) {
    stop();
    auto p_this = shared_from_this();
    // schedule task
    pThreadPoolTask = pThreadPool->scheduleAtFixedRate(0, Configuration::TELEMETRY_DATA_MONITOR_FREQUENCE, lambda);
}

} // namespace xpum