/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file standby.h
 */

#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "level_zero/ze_api.h"
#include "level_zero/zes_api.h"

namespace xpum {

class Standby {
   public:
    Standby(zes_standby_type_t type, bool on_subdevice, uint32_t subdevice_id, zes_standby_promo_mode_t mode);

    virtual ~Standby();

    zes_standby_type_t getType() const;

    bool onSubdevice() const;

    uint32_t getSubdeviceId() const;

    zes_standby_promo_mode_t getMode() const;

   private:
    zes_standby_type_t type;

    bool on_subdevice;

    uint32_t subdevice_id;

    zes_standby_promo_mode_t mode;
};

} // end namespace xpum
