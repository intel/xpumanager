#pragma once

#include <string>

#include "const.h"
#include "measurement_type.h"
#include "device_capability.h"
#include "device.h"

class Utility {
 public:
  static long long getCurrentMillisecond();

  static Timestamp_t getCurrentTime();

  static std::string getCurrentTimeString();

  static std::string getTimeString(long long milliseconds);

  static MeasurementType measurementTypeFromCapability(DeviceCapability& capability);

  static DeviceCapability capabilityFromMeasurementType(MeasurementType& measurementType);

  static std::function<void(Callback_t)> getDeviceMethod(DeviceCapability& capability, Device* p_device);

};
