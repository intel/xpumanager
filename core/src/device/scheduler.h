/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file scheduler.h
 */

#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "level_zero/ze_api.h"
#include "level_zero/zes_api.h"

namespace xpum {

struct SchedulerTimeoutMode {
    uint32_t subdevice_Id;
    zes_sched_timeout_properties_t mode_setting;
};

struct SchedulerTimesliceMode {
    uint32_t subdevice_Id;
    zes_sched_timeslice_properties_t mode_setting;
};

struct SchedulerExclusiveMode {
    uint32_t subdevice_Id;
};

class Scheduler {
   public:
    Scheduler(bool on_subdevice, uint32_t subdevice_id, bool can_control, zes_engine_type_flags_t engines, uint32_t supportedModes, zes_sched_mode_t mode,
              uint64_t val1, uint64_t val2);

    virtual ~Scheduler();

    zes_engine_type_flags_t getEngineTypes();

    uint32_t getSupportedModes();

    zes_sched_mode_t getCurrentMode();

    bool onSubdevice() const;

    uint32_t getSubdeviceId() const;

    bool canControl() const;

    uint64_t getVal1() const;
    uint64_t getVal2() const;

   private:
    zes_engine_type_flags_t engine_types;

    uint32_t supported_modes;

    zes_sched_mode_t mode;

    bool on_subdevice;

    uint32_t subdevice_id;

    bool can_control;
    uint64_t val1;
    uint64_t val2;
};

} // end namespace xpum
