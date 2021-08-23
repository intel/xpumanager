#include <chrono>

#include "device.h"
#include "utility.h"
#include "xpum_structs.h"
#include <algorithm>

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
  case DeviceCapability::TEMPERATURE:
    return  MeasurementType::TEMPERATURE;
  case DeviceCapability::MEMORY:
    return  MeasurementType::MEMORY;
  case DeviceCapability::ENGINE_UTILIZATION:
    return  MeasurementType::ENGINE_UTILIZATION;
  case DeviceCapability::METRIC_TEMPERATURE:
    return MeasurementType::METRIC_TEMPERATURE;
  case DeviceCapability::METRIC_FREQUENCY:
    return MeasurementType::METRIC_FREQUENCY;
  case DeviceCapability::METRIC_POWER:
    return MeasurementType::METRIC_POWER;
  case DeviceCapability::METRIC_ENERGY:
    return MeasurementType::METRIC_ENERGY;
  case DeviceCapability::METRIC_MEMORY_USED:
    return MeasurementType::METRIC_MEMORY_USED;
  case DeviceCapability::METRIC_MEMORY_READ:
    return MeasurementType::METRIC_MEMORY_READ;
  case DeviceCapability::METRIC_MEMORY_WRITE:
    return MeasurementType::METRIC_MEMORY_WRITE;
  case DeviceCapability::METRIC_COMPUTATION:
    return MeasurementType::METRIC_COMPUTATION;
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
  case MeasurementType::TEMPERATURE:
    return  DeviceCapability::TEMPERATURE;
  case MeasurementType::MEMORY:
    return  DeviceCapability::MEMORY;
  case MeasurementType::ENGINE_UTILIZATION:
    return  DeviceCapability::ENGINE_UTILIZATION;
  case MeasurementType::METRIC_TEMPERATURE:
    return DeviceCapability::METRIC_TEMPERATURE;
  case MeasurementType::METRIC_FREQUENCY:
    return DeviceCapability::METRIC_FREQUENCY;
  case MeasurementType::METRIC_POWER:
    return DeviceCapability::METRIC_POWER;
  case MeasurementType::METRIC_MEMORY_USED:
    return DeviceCapability::METRIC_MEMORY_USED;
  case MeasurementType::METRIC_MEMORY_READ:
    return DeviceCapability::METRIC_MEMORY_READ;
  case MeasurementType::METRIC_MEMORY_WRITE:
    return DeviceCapability::METRIC_MEMORY_WRITE;
  case MeasurementType::METRIC_COMPUTATION:
    return DeviceCapability::METRIC_COMPUTATION;
  case MeasurementType::METRIC_ENERGY:
    return DeviceCapability::METRIC_ENERGY;
  default:
    return DeviceCapability::POWER;
  }
}

std::function<void(Callback_t)> Utility::getDeviceMethod(DeviceCapability& capability, Device* p_device) {
  switch (capability) {
    case DeviceCapability::POWER:
    case DeviceCapability::METRIC_POWER:
      return [p_device](Callback_t callback){ p_device->getPower(callback); };
    case DeviceCapability::FREQUENCY:
    case DeviceCapability::METRIC_FREQUENCY:
      return [p_device](Callback_t callback){ p_device->getActuralFrequency(callback); };
    case DeviceCapability::TEMPERATURE:
    case DeviceCapability::METRIC_TEMPERATURE:
      return [p_device](Callback_t callback){ p_device->getTemperature(callback); };
    case DeviceCapability::MEMORY:
    case DeviceCapability::METRIC_MEMORY_USED:
      return [p_device](Callback_t callback){ p_device->getMemory(callback); };
    case DeviceCapability::ENGINE_UTILIZATION:
    case DeviceCapability::METRIC_COMPUTATION:
      return [p_device](Callback_t callback){ p_device->getEngineUtilization(callback); };
    case DeviceCapability::METRIC_MEMORY_READ:
      return [p_device](Callback_t callback){ p_device->getMemoryRead(callback); };
    case DeviceCapability::METRIC_MEMORY_WRITE:
      return [p_device](Callback_t callback){ p_device->getMemoryWrite(callback); };
    case DeviceCapability::METRIC_ENERGY:
      return [p_device](Callback_t callback){ p_device->getEnergy(callback); };
    default:
      break;
    }
    return nullptr;
}

bool Utility::isMetric(MeasurementType type) {
  std::vector<MeasurementType> metric_types;
  Utility::getMetricsTypes(metric_types);
  return std::find(metric_types.begin(), metric_types.end(), type) != metric_types.end();
}

bool Utility::isCounterMetric(MeasurementType type) {
  return type == MeasurementType::METRIC_ENERGY || 
         type == MeasurementType::METRIC_MEMORY_READ ||
         type == MeasurementType::METRIC_MEMORY_WRITE;
}

void Utility::getMetricsTypes(std::vector<MeasurementType>& metric_types) {
  metric_types.push_back(MeasurementType::METRIC_FREQUENCY);
  metric_types.push_back(MeasurementType::METRIC_POWER);
  metric_types.push_back(MeasurementType::METRIC_ENERGY);
  metric_types.push_back(MeasurementType::METRIC_TEMPERATURE);
  metric_types.push_back(MeasurementType::METRIC_MEMORY_USED);
  metric_types.push_back(MeasurementType::METRIC_MEMORY_READ);
  metric_types.push_back(MeasurementType::METRIC_MEMORY_WRITE);
  metric_types.push_back(MeasurementType::METRIC_COMPUTATION);
}

xpum_stats_type_t Utility::xpumStatsTypeFromMeasurementType(MeasurementType& measurementType) {
  switch (measurementType) {
  case MeasurementType::METRIC_TEMPERATURE:
    return xpum_stats_type_enum::XPUM_STATS_GPU_TEMEPERATURE;
  case MeasurementType::METRIC_FREQUENCY:
    return xpum_stats_type_enum::XPUM_STATS_GPU_FREQUENCY;
  case MeasurementType::METRIC_POWER:
    return xpum_stats_type_enum::XPUM_STATS_POWER;
  case MeasurementType::METRIC_MEMORY_USED:
    return xpum_stats_type_enum::XPUM_STATS_MEMORY_USED;
  case MeasurementType::METRIC_MEMORY_READ:
    return xpum_stats_type_enum::XPUM_STATS_MEMORY_READ;
  case MeasurementType::METRIC_MEMORY_WRITE:
    return xpum_stats_type_enum::XPUM_STATS_MEMORY_WRITE;
  case MeasurementType::METRIC_COMPUTATION:
    return xpum_stats_type_enum::XPUM_STATS_GPU_COMPUTATION;
  case MeasurementType::METRIC_ENERGY:
    return xpum_stats_type_enum::XPUM_STATS_ENERGY;
  default:
    return xpum_stats_type_enum::XPUM_STATS_POWER;
  }
}

