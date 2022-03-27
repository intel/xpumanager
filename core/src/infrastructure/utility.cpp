/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file utility.cpp
 */

#include "utility.h"

#include <algorithm>
#include <chrono>

#include "../include/xpum_structs.h"
#include "device/device.h"

namespace xpum {

long long Utility::getCurrentMillisecond() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

Timestamp_t Utility::getCurrentTime() {
    return static_cast<Timestamp_t>(getCurrentMillisecond());
}

std::string Utility::getCurrentTimeString() {
    return getTimeString(getCurrentMillisecond());
}

std::string Utility::getCurrentUTCTimeString() {
    return getUTCTimeString(getCurrentMillisecond());
}

std::string Utility::getUTCTimeString(uint64_t t) {
    time_t seconds = (long)t / 1000;
    int milli_seconds = t % 1000;
    tm* tm_p = gmtime(&seconds);
    if (!tm_p) return "";
    char buf[50];
    strftime(buf, sizeof(buf), "%FT%T", tm_p);
    char milli_buf[10];
    sprintf(milli_buf, "%03d", milli_seconds);
    return std::string(buf) + "." + std::string(milli_buf) + "Z";
}

std::string Utility::getTimeString(long long milliseconds) {
    const auto duration_since_epoch = std::chrono::milliseconds(milliseconds);
    const std::chrono::time_point<std::chrono::system_clock> time_point(duration_since_epoch);
    time_t time = std::chrono::system_clock::to_time_t(time_point);
    struct tm* p_tm = std::localtime(&time);
    char date[128] = {0};
    snprintf(date, 128, "%d-%02d-%02d %02d:%02d:%02d.%03d %s", p_tm->tm_year + 1900,
             (int)p_tm->tm_mon + 1, (int)p_tm->tm_mday, (int)p_tm->tm_hour,
             (int)p_tm->tm_min, (int)p_tm->tm_sec, (int)(milliseconds % 1000),
             p_tm->tm_zone);
    return std::string(date);
}

MeasurementType Utility::measurementTypeFromCapability(DeviceCapability& capability) {
    switch (capability) {
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
        case DeviceCapability::METRIC_MEMORY_READ_THROUGHPUT:
            return MeasurementType::METRIC_MEMORY_READ_THROUGHPUT;
        case DeviceCapability::METRIC_MEMORY_WRITE_THROUGHPUT:
            return MeasurementType::METRIC_MEMORY_WRITE_THROUGHPUT;
        case DeviceCapability::METRIC_COMPUTATION:
            return MeasurementType::METRIC_COMPUTATION;
        case DeviceCapability::METRIC_ENGINE_UTILIZATION:
            return MeasurementType::METRIC_ENGINE_UTILIZATION;
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
        case DeviceCapability::METRIC_EU_ACTIVE_STALL_IDLE:
            return MeasurementType::METRIC_EU_ACTIVE;
        case DeviceCapability::METRIC_RAS_ERROR:
            return MeasurementType::METRIC_RAS_ERROR_CAT_RESET;
        case DeviceCapability::METRIC_MEMORY_TEMPERATURE:
            return MeasurementType::METRIC_MEMORY_TEMPERATURE;
        case DeviceCapability::METRIC_FREQUENCY_THROTTLE:
            return MeasurementType::METRIC_FREQUENCY_THROTTLE;
        case DeviceCapability::METRIC_PCIE_READ_THROUGHPUT:
            return MeasurementType::METRIC_PCIE_READ_THROUGHPUT;
        case DeviceCapability::METRIC_PCIE_WRITE_THROUGHPUT:
            return MeasurementType::METRIC_PCIE_WRITE_THROUGHPUT;
        case DeviceCapability::METRIC_PCIE_READ:
            return MeasurementType::METRIC_PCIE_READ;
        case DeviceCapability::METRIC_PCIE_WRITE:
            return MeasurementType::METRIC_PCIE_WRITE;
        case DeviceCapability::METRIC_FABRIC_THROUGHPUT:
            return MeasurementType::METRIC_FABRIC_THROUGHPUT;
        default:
            return MeasurementType::METRIC_MAX;
    }
}

DeviceCapability Utility::capabilityFromMeasurementType(const MeasurementType& measurementType) {
    switch (measurementType) {
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
        case MeasurementType::METRIC_MEMORY_READ_THROUGHPUT:
            return DeviceCapability::METRIC_MEMORY_READ_THROUGHPUT;
        case MeasurementType::METRIC_MEMORY_WRITE_THROUGHPUT:
            return DeviceCapability::METRIC_MEMORY_WRITE_THROUGHPUT;
        case MeasurementType::METRIC_COMPUTATION:
            return DeviceCapability::METRIC_COMPUTATION;
        case MeasurementType::METRIC_ENGINE_UTILIZATION:
            return DeviceCapability::METRIC_ENGINE_UTILIZATION;
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
        case MeasurementType::METRIC_EU_ACTIVE:
            return DeviceCapability::METRIC_EU_ACTIVE_STALL_IDLE;
        case MeasurementType::METRIC_EU_STALL:
            return DeviceCapability::METRIC_EU_ACTIVE_STALL_IDLE;
        case MeasurementType::METRIC_EU_IDLE:
            return DeviceCapability::METRIC_EU_ACTIVE_STALL_IDLE;
        case MeasurementType::METRIC_RAS_ERROR_CAT_RESET:
            return DeviceCapability::METRIC_RAS_ERROR;
        case MeasurementType::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS:
            return DeviceCapability::METRIC_RAS_ERROR;
        case MeasurementType::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS:
            return DeviceCapability::METRIC_RAS_ERROR;
        case MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE:
            return DeviceCapability::METRIC_RAS_ERROR;
        case MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE:
            return DeviceCapability::METRIC_RAS_ERROR;
        case MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE:
            return DeviceCapability::METRIC_RAS_ERROR;
        case MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE:
            return DeviceCapability::METRIC_RAS_ERROR;
        case MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE:
            return DeviceCapability::METRIC_RAS_ERROR;
        case MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE:
            return DeviceCapability::METRIC_RAS_ERROR;
        case MeasurementType::METRIC_MEMORY_TEMPERATURE:
            return DeviceCapability::METRIC_MEMORY_TEMPERATURE;
        case MeasurementType::METRIC_FREQUENCY_THROTTLE:
            return DeviceCapability::METRIC_FREQUENCY_THROTTLE;
        case MeasurementType::METRIC_PCIE_READ_THROUGHPUT:
            return DeviceCapability::METRIC_PCIE_READ_THROUGHPUT;
        case MeasurementType::METRIC_PCIE_WRITE_THROUGHPUT:
            return DeviceCapability::METRIC_PCIE_WRITE_THROUGHPUT;
        case MeasurementType::METRIC_PCIE_READ:
            return DeviceCapability::METRIC_PCIE_READ;
        case MeasurementType::METRIC_PCIE_WRITE:
            return DeviceCapability::METRIC_PCIE_WRITE;
        case MeasurementType::METRIC_FABRIC_THROUGHPUT:
            return DeviceCapability::METRIC_FABRIC_THROUGHPUT;
        default:
            return DeviceCapability::DEVICE_CAPABILITY_MAX;
    }
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
           type == MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE ||
           type == MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE ||
           type == MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE ||
           type == MeasurementType::METRIC_PCIE_READ ||
           type == MeasurementType::METRIC_PCIE_WRITE;
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
    metric_types.push_back(MeasurementType::METRIC_MEMORY_READ_THROUGHPUT);
    metric_types.push_back(MeasurementType::METRIC_MEMORY_WRITE_THROUGHPUT);
    metric_types.push_back(MeasurementType::METRIC_COMPUTATION);
    metric_types.push_back(MeasurementType::METRIC_ENGINE_UTILIZATION);
    metric_types.push_back(MeasurementType::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION);
    metric_types.push_back(MeasurementType::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION);
    metric_types.push_back(MeasurementType::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION);
    metric_types.push_back(MeasurementType::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION);
    metric_types.push_back(MeasurementType::METRIC_ENGINE_GROUP_3D_ALL_UTILIZATION);
    metric_types.push_back(MeasurementType::METRIC_EU_ACTIVE);
    metric_types.push_back(MeasurementType::METRIC_EU_STALL);
    metric_types.push_back(MeasurementType::METRIC_EU_IDLE);
    metric_types.push_back(MeasurementType::METRIC_RAS_ERROR_CAT_RESET);
    metric_types.push_back(MeasurementType::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS);
    metric_types.push_back(MeasurementType::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS);
    metric_types.push_back(MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE);
    metric_types.push_back(MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE);
    metric_types.push_back(MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE);
    metric_types.push_back(MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE);
    metric_types.push_back(MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE);
    metric_types.push_back(MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE);
    metric_types.push_back(MeasurementType::METRIC_REQUEST_FREQUENCY);
    metric_types.push_back(MeasurementType::METRIC_MEMORY_TEMPERATURE);
    metric_types.push_back(MeasurementType::METRIC_FREQUENCY_THROTTLE);
    metric_types.push_back(MeasurementType::METRIC_PCIE_READ_THROUGHPUT);
    metric_types.push_back(MeasurementType::METRIC_PCIE_WRITE_THROUGHPUT);
    metric_types.push_back(MeasurementType::METRIC_PCIE_READ);
    metric_types.push_back(MeasurementType::METRIC_PCIE_WRITE);
    metric_types.push_back(MeasurementType::METRIC_FABRIC_THROUGHPUT);
}

MeasurementType Utility::measurementTypeFromXpumStatsType(xpum_stats_type_t& xpum_stats_type) {
    switch (xpum_stats_type) {
        case xpum_stats_type_enum::XPUM_STATS_GPU_CORE_TEMPERATURE:
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
        case xpum_stats_type_enum::XPUM_STATS_MEMORY_READ_THROUGHPUT:
            return MeasurementType::METRIC_MEMORY_READ_THROUGHPUT;
        case xpum_stats_type_enum::XPUM_STATS_MEMORY_WRITE_THROUGHPUT:
            return MeasurementType::METRIC_MEMORY_WRITE_THROUGHPUT;
        case xpum_stats_type_enum::XPUM_STATS_GPU_UTILIZATION:
            return MeasurementType::METRIC_COMPUTATION;
        case xpum_stats_type_enum::XPUM_STATS_ENGINE_UTILIZATION:
            return MeasurementType::METRIC_ENGINE_UTILIZATION;
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
        case xpum_stats_type_enum::XPUM_STATS_EU_ACTIVE:
            return MeasurementType::METRIC_EU_ACTIVE;
        case xpum_stats_type_enum::XPUM_STATS_EU_STALL:
            return MeasurementType::METRIC_EU_STALL;
        case xpum_stats_type_enum::XPUM_STATS_EU_IDLE:
            return MeasurementType::METRIC_EU_IDLE;
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
        case xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE:
            return MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE;
        case xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE:
            return MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE;
        case xpum_stats_type_enum::XPUM_STATS_GPU_REQUEST_FREQUENCY:
            return MeasurementType::METRIC_REQUEST_FREQUENCY;
        case xpum_stats_type_enum::XPUM_STATS_MEMORY_TEMPERATURE:
            return MeasurementType::METRIC_MEMORY_TEMPERATURE;
        case xpum_stats_type_enum::XPUM_STATS_FREQUENCY_THROTTLE:
            return MeasurementType::METRIC_FREQUENCY_THROTTLE;
        case xpum_stats_type_enum::XPUM_STATS_PCIE_READ_THROUGHPUT:
            return MeasurementType::METRIC_PCIE_READ_THROUGHPUT;
        case xpum_stats_type_enum::XPUM_STATS_PCIE_WRITE_THROUGHPUT:
            return MeasurementType::METRIC_PCIE_WRITE_THROUGHPUT;
        case xpum_stats_type_enum::XPUM_STATS_PCIE_READ:
            return MeasurementType::METRIC_PCIE_READ;
        case xpum_stats_type_enum::XPUM_STATS_PCIE_WRITE:
            return MeasurementType::METRIC_PCIE_WRITE;
        case xpum_stats_type_enum::XPUM_STATS_FABRIC_THROUGHPUT:
            return MeasurementType::METRIC_FABRIC_THROUGHPUT;
        default:
            return MeasurementType::METRIC_MAX;
    }
}

xpum_stats_type_t Utility::xpumStatsTypeFromMeasurementType(MeasurementType& measurementType) {
    switch (measurementType) {
        case MeasurementType::METRIC_TEMPERATURE:
            return xpum_stats_type_enum::XPUM_STATS_GPU_CORE_TEMPERATURE;
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
        case MeasurementType::METRIC_MEMORY_READ_THROUGHPUT:
            return xpum_stats_type_enum::XPUM_STATS_MEMORY_READ_THROUGHPUT;
        case MeasurementType::METRIC_MEMORY_WRITE_THROUGHPUT:
            return xpum_stats_type_enum::XPUM_STATS_MEMORY_WRITE_THROUGHPUT;
        case MeasurementType::METRIC_COMPUTATION:
            return xpum_stats_type_enum::XPUM_STATS_GPU_UTILIZATION;
        case MeasurementType::METRIC_ENGINE_UTILIZATION:
            return xpum_stats_type_enum::XPUM_STATS_ENGINE_UTILIZATION;
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
        case MeasurementType::METRIC_EU_ACTIVE:
            return xpum_stats_type_enum::XPUM_STATS_EU_ACTIVE;
        case MeasurementType::METRIC_EU_STALL:
            return xpum_stats_type_enum::XPUM_STATS_EU_STALL;
        case MeasurementType::METRIC_EU_IDLE:
            return xpum_stats_type_enum::XPUM_STATS_EU_IDLE;
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
        case MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE:
            return xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE;
        case MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE:
            return xpum_stats_type_enum::XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE;
        case MeasurementType::METRIC_REQUEST_FREQUENCY:
            return xpum_stats_type_enum::XPUM_STATS_GPU_REQUEST_FREQUENCY;
        case MeasurementType::METRIC_MEMORY_TEMPERATURE:
            return xpum_stats_type_enum::XPUM_STATS_MEMORY_TEMPERATURE;
        case MeasurementType::METRIC_FREQUENCY_THROTTLE:
            return xpum_stats_type_enum::XPUM_STATS_FREQUENCY_THROTTLE;
        case MeasurementType::METRIC_PCIE_READ_THROUGHPUT:
            return xpum_stats_type_enum::XPUM_STATS_PCIE_READ_THROUGHPUT;
        case MeasurementType::METRIC_PCIE_WRITE_THROUGHPUT:
            return xpum_stats_type_enum::XPUM_STATS_PCIE_WRITE_THROUGHPUT;
        case MeasurementType::METRIC_PCIE_READ:
            return xpum_stats_type_enum::XPUM_STATS_PCIE_READ;
        case MeasurementType::METRIC_PCIE_WRITE:
            return xpum_stats_type_enum::XPUM_STATS_PCIE_WRITE;
        case MeasurementType::METRIC_FABRIC_THROUGHPUT:
            return xpum_stats_type_enum::XPUM_STATS_FABRIC_THROUGHPUT;
        default:
            return xpum_stats_type_enum::XPUM_STATS_MAX;
    }
}

std::string Utility::getXpumStatsTypeString(MeasurementType type) {
    switch (type) {
        case MeasurementType::METRIC_TEMPERATURE:
            return std::string("temperature");
        case MeasurementType::METRIC_FREQUENCY:
            return std::string("frequency");
        case MeasurementType::METRIC_POWER:
            return std::string("power");
        case MeasurementType::METRIC_MEMORY_USED:
            return std::string("memory used");
        case MeasurementType::METRIC_MEMORY_UTILIZATION:
            return std::string("memory utilization");
        case MeasurementType::METRIC_MEMORY_BANDWIDTH:
            return std::string("memory bandwidth");
        case MeasurementType::METRIC_MEMORY_READ:
            return std::string("memory read");
        case MeasurementType::METRIC_MEMORY_WRITE:
            return std::string("memory write");
        case MeasurementType::METRIC_MEMORY_READ_THROUGHPUT:
            return std::string("memory read throughput");
        case MeasurementType::METRIC_MEMORY_WRITE_THROUGHPUT:
            return std::string("memory write throughput");
        case MeasurementType::METRIC_COMPUTATION:
            return std::string("GPU utilization");
        case MeasurementType::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION:
            return std::string("compute engine group utilization");
        case MeasurementType::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION:
            return std::string("media engine group utilization");
        case MeasurementType::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION:
            return std::string("copy engine group utilization");
        case MeasurementType::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION:
            return std::string("render engine group utilization");
        case MeasurementType::METRIC_ENGINE_GROUP_3D_ALL_UTILIZATION:
            return std::string("3D engine group utilization");
        case MeasurementType::METRIC_ENERGY:
            return std::string("energy");
        case MeasurementType::METRIC_EU_ACTIVE:
            return std::string("EU active");
        case MeasurementType::METRIC_EU_STALL:
            return std::string("EU stall");
        case MeasurementType::METRIC_EU_IDLE:
            return std::string("EU idle");
        case MeasurementType::METRIC_RAS_ERROR_CAT_RESET:
            return std::string("RAS reset");
        case MeasurementType::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS:
            return std::string("RAS programming errors");
        case MeasurementType::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS:
            return std::string("RAS driver errors");
        case MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE:
            return std::string("RAS cache correctable errors");
        case MeasurementType::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE:
            return std::string("RAS cache uncorrectable errors");
        case MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE:
            return std::string("RAS display correctable errors");
        case MeasurementType::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE:
            return std::string("RAS display uncorrectable errors");
        case MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE:
            return std::string("RAS non compute correctable errors");
        case MeasurementType::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE:
            return std::string("RAS non compute uncorrectable errors");
        case MeasurementType::METRIC_REQUEST_FREQUENCY:
            return std::string("request frequency");
        case MeasurementType::METRIC_MEMORY_TEMPERATURE:
            return std::string("memory temperature");
        case MeasurementType::METRIC_FREQUENCY_THROTTLE:
            return std::string("throttle frequency");
        case MeasurementType::METRIC_PCIE_READ_THROUGHPUT:
            return std::string("PCIE read throughput");
        case MeasurementType::METRIC_PCIE_WRITE_THROUGHPUT:
            return std::string("PCIE write throughput");
        case MeasurementType::METRIC_PCIE_READ:
            return std::string("PCIE read");
        case MeasurementType::METRIC_PCIE_WRITE:
            return std::string("PCIE write");
        case MeasurementType::METRIC_ENGINE_UTILIZATION:
            return std::string("engine utilization");
        case MeasurementType::METRIC_FABRIC_THROUGHPUT:
            return std::string("fabric throughput");
        default:
            return std::string("");
    }
}

xpum_engine_type_t Utility::toXPUMEngineType(zes_engine_group_t type) {
    switch (type) {
        case ZES_ENGINE_GROUP_COMPUTE_SINGLE:
            return XPUM_ENGINE_TYPE_COMPUTE;
        case ZES_ENGINE_GROUP_RENDER_SINGLE:
            return XPUM_ENGINE_TYPE_RENDER;
        case ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE:
            return XPUM_ENGINE_TYPE_DECODE;
        case ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE:
            return XPUM_ENGINE_TYPE_ENCODE;
        case ZES_ENGINE_GROUP_COPY_SINGLE:
            return XPUM_ENGINE_TYPE_COPY;
        case ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE:
            return XPUM_ENGINE_TYPE_MEDIA_ENHANCEMENT;
        case ZES_ENGINE_GROUP_3D_SINGLE:
            return XPUM_ENGINE_TYPE_3D;
        default:
            return XPUM_ENGINE_TYPE_UNKNOWN;
    }
}

zes_engine_group_t Utility::toZESEngineType(xpum_engine_type_t type) {
    switch (type) {
        case XPUM_ENGINE_TYPE_COMPUTE:
            return ZES_ENGINE_GROUP_COMPUTE_SINGLE;
        case XPUM_ENGINE_TYPE_RENDER:
            return ZES_ENGINE_GROUP_RENDER_SINGLE;
        case XPUM_ENGINE_TYPE_DECODE:
            return ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE;
        case XPUM_ENGINE_TYPE_ENCODE:
            return ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE;
        case XPUM_ENGINE_TYPE_COPY:
            return ZES_ENGINE_GROUP_COPY_SINGLE;
        case XPUM_ENGINE_TYPE_MEDIA_ENHANCEMENT:
            return ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE;
        case XPUM_ENGINE_TYPE_3D:
            return ZES_ENGINE_GROUP_3D_SINGLE;
        default:
            return ZES_ENGINE_GROUP_FORCE_UINT32;
    }
}

xpum_fabric_throughput_type_t Utility::toXPUMFabricThroughputType(FabricThroughputType type) {
    switch (type) {
        case RECEIVED:
            return XPUM_FABRIC_THROUGHPUT_TYPE_RECEIVED;
        case TRANSMITTED:
            return XPUM_FABRIC_THROUGHPUT_TYPE_TRANSMITTED;
        case RECEIVED_COUNTER:
            return XPUM_FABRIC_THROUGHPUT_TYPE_RECEIVED_COUNTER;
        case TRANSMITTED_COUNTER:
            return XPUM_FABRIC_THROUGHPUT_TYPE_TRANSMITTED_COUNTER;
        default:
            return XPUM_FABRIC_THROUGHPUT_TYPE_MAX;
    }
}

bool Utility::isATSPlatform(std::string device_name) {
    return device_name == "Intel(R) Graphics [0x020a]" || device_name == "Intel(R) Graphics [0x56c0]" || device_name == "Intel(R) Graphics [0x56c1]";
}

} // end namespace xpum
