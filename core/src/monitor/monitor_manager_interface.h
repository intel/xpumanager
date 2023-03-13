/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file monitor_manager_interface.h
 */

#pragma once

#include <vector>

#include "infrastructure/init_close_interface.h"
#include "infrastructure/measurement_type.h"

namespace xpum {

class MonitorManagerInterface : public InitCloseInterface {
   public:
    virtual ~MonitorManagerInterface(){};
    virtual void resetMetricTasksFrequency() = 0;
    virtual bool initOneTimeMetricMonitorTasks(MeasurementType type) = 0;
};

} // end namespace xpum
