#pragma once

#include <string>

#include "../include/xpum_structs.h"
#include "const.h"
#include "device/device.h"
#include "device_capability.h"
#include "measurement_type.h"

namespace xpum {

class Utility {
   public:
    static long long getCurrentMillisecond();

    static Timestamp_t getCurrentTime();

    static std::string getCurrentTimeString();

    static std::string getCurrentUTCTimeString();

    static std::string getTimeString(long long milliseconds);

    static std::string getUTCTimeString(uint64_t milliseconds);

    static MeasurementType measurementTypeFromCapability(DeviceCapability& capability);

    static DeviceCapability capabilityFromMeasurementType(const MeasurementType& measurementType);

    static xpum_stats_type_t xpumStatsTypeFromMeasurementType(MeasurementType& MeasurementType);

    static MeasurementType measurementTypeFromXpumStatsType(xpum_stats_type_t& xpum_stats_type);

    static bool isMetric(MeasurementType type);

    static bool isCounterMetric(MeasurementType type);

    static void getMetricsTypes(std::vector<MeasurementType>& metrics);

    static std::string getXpumStatsTypeString(MeasurementType type);

    static xpum_engine_type_t toXPUMEngineType(zes_engine_group_t type);
};

} // end namespace xpum
