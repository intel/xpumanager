#pragma once

#include <string>

#include "const.h"
#include "measurement_type.h"
#include "device_capability.h"
#include "device/device.h"
#include "../include/xpum_structs.h"

namespace xpum {

class Utility {
 public:
  static long long getCurrentMillisecond();

  static Timestamp_t getCurrentTime();

  static std::string getCurrentTimeString();

  static std::string getTimeString(long long milliseconds);

  static MeasurementType measurementTypeFromCapability(DeviceCapability& capability);

  static DeviceCapability capabilityFromMeasurementType(MeasurementType& measurementType);

  static std::function<void(Callback_t)> getDeviceMethod(DeviceCapability& capability, Device* p_device);

  static xpum_stats_type_t xpumStatsTypeFromMeasurementType(MeasurementType& MeasurementType);

  static MeasurementType measurementTypeFromXpumStatsType(xpum_stats_type_t& xpum_stats_type);

  static bool isMetric(MeasurementType type);

  static bool isCounterMetric(MeasurementType type);

  static void getMetricsTypes(std::vector<MeasurementType>& metrics);
};

} // end namespace xpum
