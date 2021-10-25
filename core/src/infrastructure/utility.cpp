#include <chrono>

#include "device/device.h"
#include "utility.h"
#include "../include/xpum_structs.h"
#include <algorithm>

namespace xpum {

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
  case DeviceCapability::METRIC_REQUEST_FREQUENCY:
    return MeasurementType::METRIC_REQUEST_FREQUENCY;
  case DeviceCapability::METRIC_POWER:
    return MeasurementType::METRIC_POWER;
  case DeviceCapability::METRIC_ENERGY:
    return MeasurementType::METRIC_ENERGY;
  case DeviceCapability::METRIC_MEMORY_USED:
    return MeasurementType::METRIC_MEMORY_USED;
  case DeviceCapability::METRIC_MEMORY_UTILIZATION:
    return MeasurementType::METRIC_MEMORY_UTILIZATION;
  case DeviceCapability::METRIC_MEMORY_BANDWIDTH:
    return MeasurementType::METRIC_MEMORY_BANDWIDTH;
  case DeviceCapability::METRIC_MEMORY_READ:
    return MeasurementType::METRIC_MEMORY_READ;
  case DeviceCapability::METRIC_MEMORY_WRITE:
    return MeasurementType::METRIC_MEMORY_WRITE;
  case DeviceCapability::METRIC_COMPUTATION:
    return MeasurementType::METRIC_COMPUTATION;
  case DeviceCapability::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION:
    return MeasurementType::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION;
  case DeviceCapability::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION:
    return MeasurementType::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION;
  case DeviceCapability::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION:
    return MeasurementType::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION;
  case DeviceCapability::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION:
    return MeasurementType::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION;
  case DeviceCapability::METRIC_ENGINE_GROUP_3D_ALL_UTILIZATION:
    return MeasurementType::METRIC_ENGINE_GROUP_3D_ALL_UTILIZATION;
  case DeviceCapability::METRIC_OCCUPATION:
    return MeasurementType::METRIC_OCCUPATION;
  case DeviceCapability::METRIC_ISSUE_EFFICIENCY:
    return MeasurementType::METRIC_ISSUE_EFFICIENCY;
  case DeviceCapability::METRIC_EXECUTION_EFFICIENCY:
    return MeasurementType::METRIC_EXECUTION_EFFICIENCY;
  case DeviceCapability::METRIC_NON_OCCUPATION:
    return MeasurementType::METRIC_NON_OCCUPATION;

  //
  case DeviceCapability::METRIC_RAS_ERROR_CAT_RESET:
    return MeasurementType::METRIC_RAS_ERROR_CAT_RESET;
  case DeviceCapability::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS:
      return MeasurementType::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS;
  case DeviceCapability::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS:
      return MeasurementType::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS;
  case DeviceCapability::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE:
      return MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE;
  case DeviceCapability::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE:
      return MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE;
  case DeviceCapability::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE:
      return MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE;
  case DeviceCapability::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE:
      return MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE;
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
  case MeasurementType::METRIC_REQUEST_FREQUENCY:
    return DeviceCapability::METRIC_REQUEST_FREQUENCY;
  case MeasurementType::METRIC_POWER:
    return DeviceCapability::METRIC_POWER;
  case MeasurementType::METRIC_MEMORY_USED:
    return DeviceCapability::METRIC_MEMORY_USED;
  case MeasurementType::METRIC_MEMORY_UTILIZATION:
    return DeviceCapability::METRIC_MEMORY_UTILIZATION;
  case MeasurementType::METRIC_MEMORY_BANDWIDTH:
    return DeviceCapability::METRIC_MEMORY_BANDWIDTH;
  case MeasurementType::METRIC_MEMORY_READ:
    return DeviceCapability::METRIC_MEMORY_READ;
  case MeasurementType::METRIC_MEMORY_WRITE:
    return DeviceCapability::METRIC_MEMORY_WRITE;
  case MeasurementType::METRIC_COMPUTATION:
    return DeviceCapability::METRIC_COMPUTATION;
  case MeasurementType::METRIC_ENERGY:
    return DeviceCapability::METRIC_ENERGY;
  case MeasurementType::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION:
    return DeviceCapability::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION;
  case MeasurementType::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION:
    return DeviceCapability::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION;
  case MeasurementType::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION:
    return DeviceCapability::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION;
  case MeasurementType::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION:
    return DeviceCapability::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION;
  case MeasurementType::METRIC_ENGINE_GROUP_3D_ALL_UTILIZATION:
    return DeviceCapability::METRIC_ENGINE_GROUP_3D_ALL_UTILIZATION;
  case MeasurementType::METRIC_OCCUPATION:
    return DeviceCapability::METRIC_OCCUPATION;
  case MeasurementType::METRIC_ISSUE_EFFICIENCY:
    return DeviceCapability::METRIC_ISSUE_EFFICIENCY;
  case MeasurementType::METRIC_EXECUTION_EFFICIENCY:
    return DeviceCapability::METRIC_EXECUTION_EFFICIENCY;
  case MeasurementType::METRIC_NON_OCCUPATION:
    return DeviceCapability::METRIC_NON_OCCUPATION;

  //
  case MeasurementType::METRIC_RAS_ERROR_CAT_RESET:
      return DeviceCapability::METRIC_RAS_ERROR_CAT_RESET;
  case MeasurementType::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS:
      return DeviceCapability::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS;
  case MeasurementType::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS:
      return DeviceCapability::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS;
  case MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE:
      return DeviceCapability::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE;
  case MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE:
      return DeviceCapability::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE;
  case MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE:
      return DeviceCapability::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE;
  case MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE:
      return DeviceCapability::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE;
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
    case DeviceCapability::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION:
      return [p_device](Callback_t callback){ p_device->getEngineGroupUtilization(callback, ZES_ENGINE_GROUP_COMPUTE_ALL); };
    case DeviceCapability::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION:
      return [p_device](Callback_t callback){ p_device->getEngineGroupUtilization(callback, ZES_ENGINE_GROUP_MEDIA_ALL); };
    case DeviceCapability::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION:
      return [p_device](Callback_t callback){ p_device->getEngineGroupUtilization(callback, ZES_ENGINE_GROUP_COPY_ALL); };
    case DeviceCapability::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION:
      return [p_device](Callback_t callback){ p_device->getEngineGroupUtilization(callback, ZES_ENGINE_GROUP_RENDER_ALL); };
    case DeviceCapability::METRIC_ENGINE_GROUP_3D_ALL_UTILIZATION:
      return [p_device](Callback_t callback){ p_device->getEngineGroupUtilization(callback, ZES_ENGINE_GROUP_3D_ALL); };
    case DeviceCapability::METRIC_MEMORY_READ:
      return [p_device](Callback_t callback){ p_device->getMemoryRead(callback); };
    case DeviceCapability::METRIC_MEMORY_WRITE:
      return [p_device](Callback_t callback){ p_device->getMemoryWrite(callback); };
    case DeviceCapability::METRIC_ENERGY:
      return [p_device](Callback_t callback){ p_device->getEnergy(callback); };
    case DeviceCapability::METRIC_MEMORY_UTILIZATION:
      return [p_device](Callback_t callback){ p_device->getMemoryUtilization(callback); };
    case DeviceCapability::METRIC_MEMORY_BANDWIDTH:
      return [p_device](Callback_t callback){ p_device->getMemoryBandwidth(callback); };
    case DeviceCapability::METRIC_OCCUPATION:
      return [p_device](Callback_t callback){ p_device->getOccupationEfficiency(callback, MeasurementType::METRIC_OCCUPATION); };
    case DeviceCapability::METRIC_ISSUE_EFFICIENCY:
      return [p_device](Callback_t callback){ p_device->getOccupationEfficiency(callback, MeasurementType::METRIC_ISSUE_EFFICIENCY); };
    case DeviceCapability::METRIC_EXECUTION_EFFICIENCY:
      return [p_device](Callback_t callback){ p_device->getOccupationEfficiency(callback, MeasurementType::METRIC_EXECUTION_EFFICIENCY); };
    case DeviceCapability::METRIC_NON_OCCUPATION:
      return [p_device](Callback_t callback){ p_device->getOccupationEfficiency(callback, MeasurementType::METRIC_NON_OCCUPATION); };
    case DeviceCapability::METRIC_RAS_ERROR_CAT_RESET:
        return [p_device](Callback_t callback) { p_device->getRasError(callback,ZES_RAS_ERROR_CAT_RESET,ZES_RAS_ERROR_TYPE_UNCORRECTABLE); };
    case DeviceCapability::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS:
        return [p_device](Callback_t callback) { p_device->getRasError(callback,ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS,ZES_RAS_ERROR_TYPE_UNCORRECTABLE); };
    case DeviceCapability::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS:
        return [p_device](Callback_t callback) { p_device->getRasError(callback,ZES_RAS_ERROR_CAT_DRIVER_ERRORS,ZES_RAS_ERROR_TYPE_UNCORRECTABLE); };
    case DeviceCapability::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE:
        return [p_device](Callback_t callback) { p_device->getRasError(callback,ZES_RAS_ERROR_CAT_CACHE_ERRORS,ZES_RAS_ERROR_TYPE_CORRECTABLE); };
    case DeviceCapability::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE:
        return [p_device](Callback_t callback) { p_device->getRasError(callback,ZES_RAS_ERROR_CAT_CACHE_ERRORS,ZES_RAS_ERROR_TYPE_UNCORRECTABLE); };
    case DeviceCapability::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE:
        return [p_device](Callback_t callback) { p_device->getRasError(callback,ZES_RAS_ERROR_CAT_DISPLAY_ERRORS,ZES_RAS_ERROR_TYPE_CORRECTABLE); };
    case DeviceCapability::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE:
        return [p_device](Callback_t callback) { p_device->getRasError(callback,ZES_RAS_ERROR_CAT_DISPLAY_ERRORS,ZES_RAS_ERROR_TYPE_UNCORRECTABLE); };
    case DeviceCapability::METRIC_REQUEST_FREQUENCY:
      return [p_device](Callback_t callback){ p_device->getRequestFrequency(callback); };
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
         type == MeasurementType::METRIC_MEMORY_WRITE ||
         type == MeasurementType::METRIC_RAS_ERROR_CAT_RESET ||
         type == MeasurementType::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS ||
         type == MeasurementType::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS ||
         type == MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE ||
         type == MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE ||
         type == MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE ||
         type == MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE
         ;
}

void Utility::getMetricsTypes(std::vector<MeasurementType>& metric_types) {
  metric_types.push_back(MeasurementType::METRIC_FREQUENCY);
  metric_types.push_back(MeasurementType::METRIC_POWER);
  metric_types.push_back(MeasurementType::METRIC_ENERGY);
  metric_types.push_back(MeasurementType::METRIC_TEMPERATURE);
  metric_types.push_back(MeasurementType::METRIC_MEMORY_USED);
  metric_types.push_back(MeasurementType::METRIC_MEMORY_UTILIZATION);
  metric_types.push_back(MeasurementType::METRIC_MEMORY_BANDWIDTH);
  metric_types.push_back(MeasurementType::METRIC_MEMORY_READ);
  metric_types.push_back(MeasurementType::METRIC_MEMORY_WRITE);
  metric_types.push_back(MeasurementType::METRIC_COMPUTATION);
  metric_types.push_back(MeasurementType::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION);
  metric_types.push_back(MeasurementType::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION);
  metric_types.push_back(MeasurementType::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION);
  metric_types.push_back(MeasurementType::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION);
  metric_types.push_back(MeasurementType::METRIC_ENGINE_GROUP_3D_ALL_UTILIZATION);
  metric_types.push_back(MeasurementType::METRIC_OCCUPATION);
  metric_types.push_back(MeasurementType::METRIC_ISSUE_EFFICIENCY);
  metric_types.push_back(MeasurementType::METRIC_EXECUTION_EFFICIENCY);
  metric_types.push_back(MeasurementType::METRIC_NON_OCCUPATION);
  metric_types.push_back(MeasurementType::METRIC_RAS_ERROR_CAT_RESET);
  metric_types.push_back(MeasurementType::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS);
  metric_types.push_back(MeasurementType::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS);
  metric_types.push_back(MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE);
  metric_types.push_back(MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE);
  metric_types.push_back(MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE);
  metric_types.push_back(MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE);
  metric_types.push_back(MeasurementType::METRIC_REQUEST_FREQUENCY);
}

MeasurementType Utility::measurementTypeFromXpumStatsType(xpum_stats_type_t& xpum_stats_type) {
  switch (xpum_stats_type) {
  case xpum_stats_type_enum::XPUM_STATS_GPU_TEMEPERATURE:
    return MeasurementType::METRIC_TEMPERATURE;
  case xpum_stats_type_enum::XPUM_STATS_GPU_FREQUENCY:
    return MeasurementType::METRIC_FREQUENCY;
  case xpum_stats_type_enum::XPUM_STATS_POWER:
    return MeasurementType::METRIC_POWER;
  case xpum_stats_type_enum::XPUM_STATS_MEMORY_USED:
    return MeasurementType::METRIC_MEMORY_USED;
  case xpum_stats_type_enum::XPUM_STATS_MEMORY_UTILIZATION:
    return MeasurementType::METRIC_MEMORY_UTILIZATION;
  case xpum_stats_type_enum::XPUM_STATS_MEMORY_BANDWIDTH:
    return MeasurementType::METRIC_MEMORY_BANDWIDTH;
  case xpum_stats_type_enum::XPUM_STATS_MEMORY_READ:
    return MeasurementType::METRIC_MEMORY_READ;
  case xpum_stats_type_enum::XPUM_STATS_MEMORY_WRITE:
    return MeasurementType::METRIC_MEMORY_WRITE;
  case xpum_stats_type_enum::XPUM_STATS_GPU_UTILIZATION:
    return MeasurementType::METRIC_COMPUTATION;
  case xpum_stats_type_enum::XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION:
    return MeasurementType::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION;
  case xpum_stats_type_enum::XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION:
    return MeasurementType::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION;
  case xpum_stats_type_enum::XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION:
    return MeasurementType::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION;
  case xpum_stats_type_enum::XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION:
    return MeasurementType::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION;
  case xpum_stats_type_enum::XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION:
    return MeasurementType::METRIC_ENGINE_GROUP_3D_ALL_UTILIZATION;
  case xpum_stats_type_enum::XPUM_STATS_ENERGY:
    return MeasurementType::METRIC_ENERGY;
  case xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_RESET:
      return MeasurementType::METRIC_RAS_ERROR_CAT_RESET;
  case xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS:
      return MeasurementType::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS;
  case xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS:
      return MeasurementType::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS;
  case xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE:
      return MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE;
  case xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE:
      return MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE;
  case xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE:
      return MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE;
  case xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE:
      return MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE;
  case xpum_stats_type_enum::XPUM_STATS_GPU_REQUEST_FREQUENCY:
    return MeasurementType::METRIC_REQUEST_FREQUENCY;
  default:
    return MeasurementType::METRIC_POWER;
  }
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
  case MeasurementType::METRIC_MEMORY_UTILIZATION:
    return xpum_stats_type_enum::XPUM_STATS_MEMORY_UTILIZATION;
  case MeasurementType::METRIC_MEMORY_BANDWIDTH:
    return xpum_stats_type_enum::XPUM_STATS_MEMORY_BANDWIDTH;
  case MeasurementType::METRIC_MEMORY_READ:
    return xpum_stats_type_enum::XPUM_STATS_MEMORY_READ;
  case MeasurementType::METRIC_MEMORY_WRITE:
    return xpum_stats_type_enum::XPUM_STATS_MEMORY_WRITE;
  case MeasurementType::METRIC_COMPUTATION:
    return xpum_stats_type_enum::XPUM_STATS_GPU_UTILIZATION;
  case MeasurementType::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION:
    return xpum_stats_type_enum::XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION;
  case MeasurementType::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION:
    return xpum_stats_type_enum::XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION;
  case MeasurementType::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION:
    return xpum_stats_type_enum::XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION;
  case MeasurementType::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION:
    return xpum_stats_type_enum::XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION;
  case MeasurementType::METRIC_ENGINE_GROUP_3D_ALL_UTILIZATION:
    return xpum_stats_type_enum::XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION;
  case MeasurementType::METRIC_ENERGY:
    return xpum_stats_type_enum::XPUM_STATS_ENERGY;
  case MeasurementType::METRIC_OCCUPATION:
    return xpum_stats_type_enum::XPUM_STATS_OCCUPATION;
  case MeasurementType::METRIC_ISSUE_EFFICIENCY:
    return xpum_stats_type_enum::XPUM_STATS_ISSUE_EFFICIENCY;
  case MeasurementType::METRIC_EXECUTION_EFFICIENCY:
    return xpum_stats_type_enum::XPUM_STATS_EXECUTION_EFFICIENCY;
  case MeasurementType::METRIC_NON_OCCUPATION:
    return xpum_stats_type_enum::XPUM_STATS_NON_OCCUPATION;
  case MeasurementType::METRIC_RAS_ERROR_CAT_RESET:
      return xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_RESET;
  case MeasurementType::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS:
      return xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS;
  case MeasurementType::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS:
      return xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS;
  case MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE:
      return xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE;
  case MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE:
      return xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE;
  case MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE:
      return xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE;
  case MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE:
      return xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE;
  case MeasurementType::METRIC_REQUEST_FREQUENCY:
    return xpum_stats_type_enum::XPUM_STATS_GPU_REQUEST_FREQUENCY;
  default:
    return xpum_stats_type_enum::XPUM_STATS_POWER;
  }
}


} // end namespace xpum
