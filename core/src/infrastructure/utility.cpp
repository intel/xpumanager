#include <chrono>

#include "device.h"
#include "utility.h"

long long Utility::getCurrentMillisecond() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();  
}

Timestamp_t Utility::getCurrentTime() {
  return static_cast<Timestamp_t>(getCurrentMillisecond());
}

std::string Utility::getCurrentTimeString() {
  return getTimeString(getCurrentMillisecond());
}

std::string Utility::getTimeString(long long milliseconds) {
  const auto duration_since_epoch = std::chrono::milliseconds(milliseconds);
  const std::chrono::time_point<std::chrono::system_clock> time_point(duration_since_epoch);
  time_t time = std::chrono::system_clock::to_time_t(time_point);
  struct tm* p_tm = std::localtime(&time);
  char date[32] = {0};
  sprintf(date, "%d-%02d-%02d %02d:%02d:%02d.%03d %s", p_tm->tm_year + 1900,
          (int)p_tm->tm_mon + 1, (int)p_tm->tm_mday, (int)p_tm->tm_hour,
          (int)p_tm->tm_min, (int)p_tm->tm_sec, (int)(milliseconds % 1000),
          p_tm->tm_zone);
  return std::string(date);
}

MeasurementType Utility::measurementTypeFromCapability(DeviceCapability& capability) {
  switch (capability) {
  case DeviceCapability::POWER: 
    return  MeasurementType::POWER;
  case DeviceCapability::FREQUENCY: 
    return  MeasurementType::FREQUENCY;
  default:
    return MeasurementType::POWER;
  }
}

DeviceCapability Utility::capabilityFromMeasurementType(MeasurementType& measurementType) {
  switch (measurementType) {
  case MeasurementType::POWER: 
    return  DeviceCapability::POWER;
  case MeasurementType::FREQUENCY: 
    return  DeviceCapability::FREQUENCY;
  default:
    return DeviceCapability::POWER;
  }
}

std::function<void(Callback_t)> Utility::getDeviceMethod(DeviceCapability& capability, Device* p_device) {
  switch (capability) {
    case DeviceCapability::POWER: return [p_device](Callback_t callback){ p_device->getPower(callback); };
    case DeviceCapability::FREQUENCY: return [p_device](Callback_t callback){ p_device->getActuralFrequency(callback); };
    default:
      break;
    }
    return nullptr;
}


