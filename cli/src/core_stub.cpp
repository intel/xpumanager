/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file core_stub.cpp
 */

#include "core_stub.h"

#include <grpc++/grpc++.h>

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <map>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>

#include "xpum_structs.h"

namespace xpum::cli {

std::string CoreStub::isotimestamp(uint64_t t, bool withoutDate) {
    time_t seconds = (long)t / 1000;
    int milli_seconds = t % 1000;
    tm* tm_p = gmtime(&seconds);
    char buf[50];
    char milli_buf[10];
    sprintf(milli_buf, "%03d", milli_seconds);
    if (withoutDate) {
        strftime(buf, sizeof(buf), "%T", tm_p);
        return std::string(buf) + "." + std::string(milli_buf);
    }
    else {
        strftime(buf, sizeof(buf), "%FT%T", tm_p);
        return std::string(buf) + "." + std::string(milli_buf) + "Z";
    }
}

struct MetricsTypeEntry {
    xpum_stats_type_t key;
    std::string keyStr;
};

static MetricsTypeEntry metricsTypeArray[]{
    {XPUM_STATS_GPU_UTILIZATION, "XPUM_STATS_GPU_UTILIZATION"},
    {XPUM_STATS_EU_ACTIVE, "XPUM_STATS_EU_ACTIVE"},
    {XPUM_STATS_EU_STALL, "XPUM_STATS_EU_STALL"},
    {XPUM_STATS_EU_IDLE, "XPUM_STATS_EU_IDLE"},
    {XPUM_STATS_POWER, "XPUM_STATS_POWER"},
    {XPUM_STATS_ENERGY, "XPUM_STATS_ENERGY"},
    {XPUM_STATS_GPU_FREQUENCY, "XPUM_STATS_GPU_FREQUENCY"},
    {XPUM_STATS_GPU_CORE_TEMPERATURE, "XPUM_STATS_GPU_CORE_TEMPERATURE"},
    {XPUM_STATS_MEMORY_USED, "XPUM_STATS_MEMORY_USED"},
    {XPUM_STATS_MEMORY_UTILIZATION, "XPUM_STATS_MEMORY_UTILIZATION"},
    {XPUM_STATS_MEMORY_BANDWIDTH, "XPUM_STATS_MEMORY_BANDWIDTH"},
    {XPUM_STATS_MEMORY_READ, "XPUM_STATS_MEMORY_READ"},
    {XPUM_STATS_MEMORY_WRITE, "XPUM_STATS_MEMORY_WRITE"},
    {XPUM_STATS_MEMORY_READ_THROUGHPUT, "XPUM_STATS_MEMORY_READ_THROUGHPUT"},
    {XPUM_STATS_MEMORY_WRITE_THROUGHPUT, "XPUM_STATS_MEMORY_WRITE_THROUGHPUT"},
    {XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION"},
    {XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION"},
    {XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION"},
    {XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION"},
    {XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION"},
    {XPUM_STATS_RAS_ERROR_CAT_RESET, "XPUM_STATS_RAS_ERROR_CAT_RESET"},
    {XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS, "XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS"},
    {XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS, "XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS"},
    {XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE"},
    {XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE"},
    {XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE"},
    {XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE"},
    {XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE"},
    {XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE"},
    {XPUM_STATS_GPU_REQUEST_FREQUENCY, "XPUM_STATS_GPU_REQUEST_FREQUENCY"},
    {XPUM_STATS_MEMORY_TEMPERATURE, "XPUM_STATS_MEMORY_TEMPERATURE"},
    {XPUM_STATS_FREQUENCY_THROTTLE, "XPUM_STATS_FREQUENCY_THROTTLE"},
    {XPUM_STATS_PCIE_READ_THROUGHPUT, "XPUM_STATS_PCIE_READ_THROUGHPUT"},
    {XPUM_STATS_PCIE_WRITE_THROUGHPUT, "XPUM_STATS_PCIE_WRITE_THROUGHPUT"},
    {XPUM_STATS_PCIE_READ, "XPUM_STATS_PCIE_READ"},
    {XPUM_STATS_PCIE_WRITE, "XPUM_STATS_PCIE_WRITE"},
    {XPUM_STATS_ENGINE_UTILIZATION, "XPUM_STATS_ENGINE_UTILIZATION"}};

std::string CoreStub::metricsTypeToString(xpum_stats_type_t metricsType) {
    for (auto item : metricsTypeArray) {
        if (item.key == metricsType) {
            return item.keyStr;
        }
    }
    return std::to_string(metricsType);
}

std::string CoreStub::getCardUUID(const std::string& rawUUID) {
    std::string::size_type start = rawUUID.find_last_of('-');
    if (start != std::string::npos) {
        return rawUUID.substr(start + 1, rawUUID.length());
    } else {
        return rawUUID;
    }
}

std::string CoreStub::healthStatusEnumToString(HealthStatusType status) {
    std::string ret;
    switch (status) {
        case HEALTH_STATUS_UNKNOWN:
            ret = "Unknown";
            break;
        case HEALTH_STATUS_OK:
            ret = "OK";
            break;
        case HEALTH_STATUS_WARNING:
            ret = "Warning";
            break;
        case HEALTH_STATUS_CRITICAL:
            ret = "Critical";
            break;
        default:
            break;
    }
    return ret;
}

std::string CoreStub::healthTypeEnumToString(HealthType type) {
    std::string ret;
    switch (type) {
        case HEALTH_CORE_THERMAL:
            ret = "core_temperature";
            break;
        case HEALTH_MEMORY_THERMAL:
            ret = "memory_temperature";
            break;
        case HEALTH_POWER:
            ret = "power";
            break;
        case HEALTH_MEMORY:
            ret = "memory";
            break;
        case HEALTH_FABRIC_PORT:
            ret = "xe_link_port";
            break;
        case HEALTH_FREQUENCY:
            ret = "frequency";
            break;
        default:
            break;
    }
    return ret;
}

std::string CoreStub::schedulerModeToString(int mode) {
    std::string ret = "null"; //"SCHEDULER_MODE_NULL";
    switch (mode) {
        case 0:
            ret = "timeout"; //"SCHEDULER_MODE_TIMEOUT";
            break;
        case 1:
            ret = "timeslice"; //"SCHEDULER_MODE_TIMESLICE";
            break;
        case 2:
            ret = "exclusive"; //"SCHEDULER_MODE_EXCLUSIVE";
            break;
        case 3:
            ret = "debug"; //"SCHEDULER_MODE_DEBUG";
            break;
        default:
            break;
    }
    return ret;
}
std::string CoreStub::standbyModeToString(int mode) {
    std::string ret = "null"; //"STANDBY_MODE_NULL";
    switch (mode) {
        case 0:
            ret = "default"; //"STANDBY_MODE_DEFAULT";
            break;
        case 1:
            ret = "never"; //"STANDBY_MODE_NEVER";
            break;
        default:
            break;
    }
    return ret;
}

std::string CoreStub::policyTypeEnumToString(XpumPolicyType type) {
    //std::string ret = "POLICY_TYPE_MAX";
    std::string ret = "Error: cli unsupport this type";
    switch (type) {
        case POLICY_TYPE_GPU_TEMPERATURE:
            ret = "1. GPU Core Temperature";
            break;
        case POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS:
            ret = "2. Programming Errors";
            break;
        case POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS:
            ret = "3. Driver Errors";
            break;
        case POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE:
            ret = "4. Cache Errors Correctable";
            break;
        case POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE:
            ret = "5. Cache Errors Uncorrectable";
            break;
        // case POLICY_TYPE_GPU_MEMORY_TEMPERATURE:
        //     ret = "POLICY_TYPE_GPU_MEMORY_TEMPERATURE";
        //     break;
        // case POLICY_TYPE_GPU_POWER:
        //     ret = "POLICY_TYPE_GPU_POWER";
        //     break;
        // case POLICY_TYPE_RAS_ERROR_CAT_RESET:
        //     ret = "POLICY_TYPE_RAS_ERROR_CAT_RESET";
        //     break;
        // case POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE:
        //     ret = "POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE";
        //     break;
        // case POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE:
        //     ret = "POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE";
        //     break;
        default:
            break;
    }
    return ret;
}

std::string CoreStub::policyConditionTypeEnumToString(XpumPolicyConditionType type) {
    //std::string ret = "POLICY_CONDITION_TYPE_GREATER";
    std::string ret = "1. More than";
    switch (type) {
        case POLICY_CONDITION_TYPE_GREATER:
            ret = "1. More than";
            break;
        case POLICY_CONDITION_TYPE_LESS:
            ret = "3. Less than";
            break;
        case POLICY_CONDITION_TYPE_WHEN_INCREASE:
            ret = "2. When occur";
            break;
        default:
            break;
    }
    return ret;
}

std::string CoreStub::policyActionTypeEnumToString(XpumPolicyActionType type) {
    std::string ret = "4. No action";
    switch (type) {
        case POLICY_ACTION_TYPE_NULL:
            ret = "3. Notify";
            break;
        case POLICY_ACTION_TYPE_THROTTLE_DEVICE:
            ret = "1. Throttle GPU Core Frequency";
            break;
        // case POLICY_ACTION_TYPE_RESET_DEVICE:
        //     ret = "2. Reset GPU";
        //     break;
        default:
            break;
    }
    return ret;
}

bool CoreStub::isCliSupportedPolicyType(XpumPolicyType type) {
    if (type == XpumPolicyType::POLICY_TYPE_GPU_TEMPERATURE || type == XpumPolicyType::POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS || type == XpumPolicyType::POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS || type == XpumPolicyType::POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE || type == XpumPolicyType::POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE) {
        return true;
    }
    return false;
}

} // end namespace xpum::cli
