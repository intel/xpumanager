/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file configuration.h
 */

#pragma once

#include "infrastructure/logger.h"
#include "measurement_type.h"
#include "xpum_structs.h"

#include <set>

namespace xpum {

    class Configuration {
    public:
        static int POWER_MONITOR_INTERNAL_PERIOD;
        static int MEMORY_BANDWIDTH_MONITOR_INTERNAL_PERIOD;
        static int MEMORY_READ_WRITE_INTERNAL_PERIOD;
        static int ENGINE_GPU_UTILIZATION_INTERNAL_PERIOD;
        static uint32_t DEFAULT_MEASUREMENT_DATA_SCALE;
        static int EU_ACTIVE_STALL_IDLE_MONITOR_INTERNAL_PERIOD;
        static int EU_ACTIVE_STALL_IDLE_STREAMER_SAMPLING_PERIOD;

        static void init() {
            initEnabledMetrics();
        }

        static void initEnabledMetrics();

        static std::set<MeasurementType>& getEnabledMetrics() {
            return enabled_metrics;
        }

    private:
        static std::set<MeasurementType> enabled_metrics;
    };

} // end namespace xpum
