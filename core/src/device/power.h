/* 
 *  Copyright (C) 2021-2025 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file power.h
 */

#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "level_zero/ze_api.h"
#include "level_zero/zes_api.h"

namespace xpum {

struct Power_limit_ext_t {
    int32_t limit; //in milliwatts
    int32_t level;
};

struct Power_sustained_limit_t {
    bool enabled;

    int32_t power;

    int32_t interval;
};

struct Power_burst_limit_t {
    bool enabled;

    int32_t power;
};

struct Power_peak_limit_t {
    int32_t power_AC;

    int32_t power_DC;
};

class Power {
   public:
    Power(bool on_subdevice, uint32_t subdevice_id, bool can_control, bool is_energy_threshold_supported, int32_t default_limit, int32_t min_limit, int32_t max_limit);

    virtual ~Power();

    bool onSubdevice();

    uint32_t getSubdeviceId();

    bool canControl();

    bool isEnergyThresholdSupported();

    int32_t getDefaultLimit();

    int32_t getMinLimit();

    int32_t getMaxLimit();

    void getPowerLimits(Power_sustained_limit_t& sustained_limit,
                        Power_burst_limit_t& burst_limit,
                        Power_peak_limit_t& peak_limit);

    void setPowerLimits(const Power_sustained_limit_t& sustained_limit,
                        const Power_burst_limit_t& burst_limit,
                        const Power_peak_limit_t& peak_limit);

   private:
    bool on_subdevice;

    uint32_t subdevice_id;

    bool can_control;

    bool is_energy_threshold_supported;

    int32_t default_limit;

    int32_t min_limit;

    int32_t max_limit;

    Power_sustained_limit_t sustained_limit;

    Power_burst_limit_t burst_limit;

    Power_peak_limit_t peak_limit;
};

} // end namespace xpum
