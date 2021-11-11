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

    static std::string getTimeString(long long milliseconds);

    static MeasurementType measurementTypeFromCapability(DeviceCapability& capability);

    static DeviceCapability capabilityFromMeasurementType(MeasurementType& measurementType);

    static xpum_stats_type_t xpumStatsTypeFromMeasurementType(MeasurementType& MeasurementType);

    static MeasurementType measurementTypeFromXpumStatsType(xpum_stats_type_t& xpum_stats_type);

    static bool isMetric(MeasurementType type);

    static bool isCounterMetric(MeasurementType type);

    static void getMetricsTypes(std::vector<MeasurementType>& metrics);
};

} // end namespace xpum
