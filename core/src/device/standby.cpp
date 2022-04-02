/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file standby.cpp
 */

#include "standby.h"

#include "infrastructure/logger.h"

namespace xpum {

Standby::Standby(zes_standby_type_t type, bool on_subdevice, uint32_t subdevice_id, zes_standby_promo_mode_t mode) {
    this->type = type;
    this->on_subdevice = on_subdevice;
    this->subdevice_id = subdevice_id;
    this->mode = mode;
}

Standby::~Standby() {
}

zes_standby_type_t Standby::getType() const {
    return type;
}

bool Standby::onSubdevice() const {
    return on_subdevice;
}

uint32_t Standby::getSubdeviceId() const {
    return subdevice_id;
}

zes_standby_promo_mode_t Standby::getMode() const {
    return mode;
}
} // end namespace xpum
