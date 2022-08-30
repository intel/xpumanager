/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file monitor_manager.h
 */

#pragma once

#include <vector>

#include "monitor_manager_interface.h"
#include "monitor_task.h"

namespace xpum {

class MonitorManager : public MonitorManagerInterface {
   public:
    MonitorManager(std::shared_ptr<DeviceManagerInterface>& p_device_manager,
                   std::shared_ptr<DataLogicInterface>& p_data_logic);

    virtual ~MonitorManager();

   public:
    void init() override;

    void close() override;

    void resetMetricTasksFrequency();

    bool initOneTimeMetricMonitorTasks(MeasurementType type);

   private:
    void createMonitorTasks(MeasurementType target_type);

   private:
    std::shared_ptr<DeviceManagerInterface> p_device_manager;

    std::shared_ptr<DataLogicInterface> p_data_logic;

    std::shared_ptr<ScheduledThreadPool> p_scheduled_thread_pool;

    std::vector<std::shared_ptr<MonitorTask>> tasks;

    std::mutex mutex;

};

} // end namespace xpum
