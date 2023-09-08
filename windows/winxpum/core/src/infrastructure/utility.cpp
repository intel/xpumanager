/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file utility.cpp
 */

#include "pch.h"
#include "utility.h"
#include "api/device_model.h"

#include <chrono>

namespace xpum {

    long long Utility::getCurrentMillisecond() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    long long Utility::getCurrentMicrosecond() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
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
        case xpum_stats_type_enum::XPUM_STATS_FREQUENCY_THROTTLE_REASON_GPU:
            return MeasurementType::METRIC_FREQUENCY_THROTTLE_REASON_GPU;
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
        case xpum_stats_type_enum::XPUM_STATS_MEDIA_ENGINE_FREQUENCY:
            return MeasurementType::METRIC_MEDIA_ENGINE_FREQUENCY;
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
        case MeasurementType::METRIC_FREQUENCY_THROTTLE_REASON_GPU:
            return xpum_stats_type_enum::XPUM_STATS_FREQUENCY_THROTTLE_REASON_GPU;
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
        case MeasurementType::METRIC_MEDIA_ENGINE_FREQUENCY:
            return xpum_stats_type_enum::XPUM_STATS_MEDIA_ENGINE_FREQUENCY;
        default:
            return xpum_stats_type_enum::XPUM_STATS_MAX;
        }
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

    bool Utility::isATSMPlatform(const zes_device_handle_t& device) {
        zes_device_properties_t props;
        props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
        props.pNext = nullptr;
        bool is_atsm = false;
        if (zesDeviceGetProperties(device, &props) == ZE_RESULT_SUCCESS) {
        int device_model = getDeviceModelByPciDeviceId(props.core.deviceId);
        is_atsm = (device_model == XPUM_DEVICE_MODEL_ATS_M_1) || (device_model == XPUM_DEVICE_MODEL_ATS_M_3);
        }
        return is_atsm;
    }
}