/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file frequency.cpp
 */

#include "frequency.h"

namespace xpum {

Frequency::Frequency(zes_freq_domain_t type,
                     bool on_subdevice,
                     uint32_t subdevice_id,
                     bool can_control,
                     bool is_throttle_event_supported,
                     double min, double max) {
    this->type = type;
    this->on_subdevice = on_subdevice;
    this->subdevice_id = subdevice_id;
    this->can_control = can_control;
    this->is_throttle_event_supported = is_throttle_event_supported;
    this->min = min;
    this->max = max;
}

Frequency::Frequency(zes_freq_domain_t type,
                     uint32_t subdevice_id,
                     double min, double max) {
    this->type = type;
    this->subdevice_id = subdevice_id;
    this->min = min;
    this->max = max;
    this->on_subdevice = false;
    this->can_control = false;
    this->is_throttle_event_supported = false; 
}

Frequency::~Frequency() {
}

zes_freq_domain_t Frequency::getType() const {
    return type;
}

int32_t Frequency::getTypeValue() const {
    return static_cast<int32_t>(type);
}

bool Frequency::onSubdevice() const {
    return on_subdevice;
}

uint32_t Frequency::getSubdeviceId() const {
    return subdevice_id;
}

bool Frequency::canControl() const {
    return can_control;
}

bool Frequency::isThrottleEventSupported() const {
    return is_throttle_event_supported;
}

double Frequency::getMin() const {
    return min;
}

double Frequency::getMax() const {
    return max;
}

} // end namespace xpum
