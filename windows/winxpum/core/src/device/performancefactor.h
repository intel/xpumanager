/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file performancefactor.h
 */

#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "level_zero/ze_api.h"
#include "level_zero/zes_api.h"

namespace xpum {

class PerformanceFactor {
   public:
    PerformanceFactor(bool on_subdevice, uint32_t subdevice_id, zes_engine_type_flags_t engine, double factor);

    virtual ~PerformanceFactor();

    zes_engine_type_flags_t getEngine() const;

    bool onSubdevice() const;

    uint32_t getSubdeviceId() const;

    double getFactor() const;

   private:
    zes_engine_type_flags_t engine;

    bool on_subdevice;

    uint32_t subdevice_id;

    double factor;
};

} // end namespace xpum
