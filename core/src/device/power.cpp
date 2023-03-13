/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file power.cpp
 */

#include "power.h"

#include "infrastructure/logger.h"

namespace xpum {

Power::Power(bool on_subdevice, uint32_t subdevice_id, bool can_control, bool is_energy_threshold_supported, int32_t default_limit, int32_t min_limit, int32_t max_limit) {
    this->on_subdevice = on_subdevice;
    this->subdevice_id = subdevice_id;
    this->can_control = can_control;
    this->is_energy_threshold_supported = is_energy_threshold_supported;
    this->default_limit = default_limit;
    this->min_limit = min_limit;
    this->max_limit = max_limit;
}

Power::~Power() {
}

bool Power::onSubdevice() {
    return on_subdevice;
}

uint32_t Power::getSubdeviceId() {
    return subdevice_id;
}

bool Power::canControl() {
    return can_control;
}

bool Power::isEnergyThresholdSupported() {
    return is_energy_threshold_supported;
}

int32_t Power::getDefaultLimit() {
    return default_limit;
}

int32_t Power::getMinLimit() {
    return min_limit;
}

int32_t Power::getMaxLimit() {
    return max_limit;
}

void Power::getPowerLimits(Power_sustained_limit_t& sustained_limit,
                           Power_burst_limit_t& burst_limit,
                           Power_peak_limit_t& peak_limit) {
    sustained_limit = this->sustained_limit;
    burst_limit = this->burst_limit;
    peak_limit = this->peak_limit;
}

void Power::setPowerLimits(const Power_sustained_limit_t& sustained_limit,
                           const Power_burst_limit_t& burst_limit,
                           const Power_peak_limit_t& peak_limit) {
    this->sustained_limit = sustained_limit;
    this->burst_limit = burst_limit;
    this->peak_limit = peak_limit;
}

} // end namespace xpum
