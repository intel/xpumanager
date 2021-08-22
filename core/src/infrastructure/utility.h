#pragma once

#include <string>

#include "const.h"
#include "measurement_type.h"
#include "device_capability.h"
#include "device.h"
#include "xpum_structs.h"

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

  static bool isMetric(MeasurementType type);

  static void getMetricsTypes(std::vector<MeasurementType>& metrics);
};
