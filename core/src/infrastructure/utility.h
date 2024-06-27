/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file utility.h
 */

#pragma once

#include <string>

#include "../include/xpum_structs.h"
#include "const.h"
#include "device/device.h"
#include "device_capability.h"
#include "measurement_type.h"

namespace xpum {

struct UEvent {
    std::string pciId;
    std::string bdf;
};

class Utility {
   public:
    static long long getCurrentMillisecond();

    static Timestamp_t getCurrentTime();

    static std::string getCurrentTimeString();

    static std::string getCurrentLocalTimeString(bool showData=false);

    static std::string getTimeString(long long milliseconds);

    static std::string getLocalTimeString(uint64_t milliseconds, bool showData=false);

    static MeasurementType measurementTypeFromCapability(DeviceCapability& capability);

    static DeviceCapability capabilityFromMeasurementType(const MeasurementType& measurementType);

    static xpum_stats_type_t xpumStatsTypeFromMeasurementType(MeasurementType& MeasurementType);

    static MeasurementType measurementTypeFromXpumStatsType(xpum_stats_type_t& xpum_stats_type);

    static bool isMetric(MeasurementType type);

    static bool isCounterMetric(MeasurementType type);

    static void getMetricsTypes(std::vector<MeasurementType>& metrics);

    static std::string getXpumStatsTypeString(MeasurementType type);

    static xpum_engine_type_t toXPUMEngineType(zes_engine_group_t type);

    static zes_engine_group_t toZESEngineType(xpum_engine_type_t type);

    static xpum_fabric_throughput_type_t toXPUMFabricThroughputType(FabricThroughputType type);

    static bool isATSMPlatform(const zes_device_handle_t &device);
    
    static bool isPVCPlatform(const zes_device_handle_t &device);

    static void parallel_in_batches(unsigned num_elements, unsigned num_threads,
                  std::function<void (int start, int end)> functor,
                  bool use_multithreading = true);
   
   static std::vector<std::string> split(const std::string &s, char delim);
   static bool getUEvent(UEvent &uevent, const char *d_name);
};

} // end namespace xpum
