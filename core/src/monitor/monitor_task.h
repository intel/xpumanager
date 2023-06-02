/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file monitor_task.h
 */

#pragma once

#include "control/device_manager_interface.h"
#include "data_logic/data_logic_interface.h"
#include "infrastructure/device_capability.h"
#include "infrastructure/scheduled_thread_pool.h"

namespace xpum {

enum MonitorTaskType {
    DEFAULT_TELEMETRY = 0,
    GPU_METRICS = 1,
    TASK_TYPE_FORCE_UINT32 = 0x7fffffff
};

class MonitorTask : public std::enable_shared_from_this<MonitorTask> {
   public:
    MonitorTask(
        DeviceCapability capability,
        int freq,
        std::shared_ptr<DeviceManagerInterface>& p_device_manager,
        std::shared_ptr<DataLogicInterface>& p_data_logic);

    MonitorTask(
        DeviceCapability capability,
        int freq,
        std::shared_ptr<DeviceManagerInterface>& p_device_manager,
        std::shared_ptr<DataLogicInterface>& p_data_logic,
        MonitorTaskType type);

    virtual ~MonitorTask();

   public:
    void start(std::shared_ptr<ScheduledThreadPool>& p_scheduled_thread_pool);

    void stop();

    DeviceCapability getCapability();

    MonitorTaskType getType();

    bool finished();

   private:
    DeviceCapability capability;
    int freq;
    std::mutex mutex;
    std::condition_variable data_cv;
    std::shared_ptr<DeviceManagerInterface> p_device_manager;
    std::shared_ptr<DataLogicInterface> p_data_logic;
    MonitorTaskType type;
    std::map<std::string, bool> monitor_task_log_status;
    std::shared_ptr<ScheduledThreadPoolTask> p_scheduled_task;
    std::atomic<int> exe_counter;
};

} // end namespace xpum
