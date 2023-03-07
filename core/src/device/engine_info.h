/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file engine_info.h
 */

#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "level_zero/ze_api.h"
#include "level_zero/zes_api.h"

namespace xpum {

class EngineInfo {
   public:
    EngineInfo();
    EngineInfo(zes_engine_group_t type, bool on_subdevice, uint32_t subdevice_id);

    EngineInfo(const EngineInfo& other);

    virtual ~EngineInfo();

    zes_engine_group_t getType() const;

    bool onSubdevice() const;

    uint32_t getSubdeviceId() const;

    uint32_t getIndex() const;

    void setIndex(uint32_t index);

   private:
    zes_engine_group_t type;

    bool on_subdevice;

    uint32_t subdevice_id;

    uint32_t index;
};

} // end namespace xpum
