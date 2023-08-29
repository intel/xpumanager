/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file utility.h
 */

#pragma once

#include <string>

#include "xpum_structs.h"
#include "device/device.h"
#include "device_capability.h"
#include "measurement_type.h"

namespace xpum {

    class Utility {
    public:
        static long long getCurrentMillisecond();

        static long long getCurrentMicrosecond();

        static xpum_stats_type_t xpumStatsTypeFromMeasurementType(MeasurementType& MeasurementType);

        static MeasurementType measurementTypeFromXpumStatsType(xpum_stats_type_t& xpum_stats_type);
        
        static bool isCounterMetric(MeasurementType type);

        static bool isATSMPlatform(const zes_device_handle_t &device);
    };

} // end namespace xpum
