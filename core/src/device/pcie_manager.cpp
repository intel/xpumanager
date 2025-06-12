/* 
 *  Copyright (C) 2021-2025 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file pcie_manager.cpp
 */

#include "pcie_manager.h"

#include "infrastructure/exception/base_exception.h"
#include "infrastructure/handle_lock.h"
#include "infrastructure/logger.h"

namespace xpum {

uint64_t PCIeManager::getLatestPCIeReadThroughput(const zes_device_handle_t& device) {
    double prevCounter = 0.0;
    double curCounter = 0.0;
    double curTime = 0.0;
    double prevTime = 0.0;
    double throughput = 0.0;
    zes_pci_stats_t stats = {};
    ze_result_t res = ZE_RESULT_SUCCESS;

    XPUM_ZE_HANDLE_LOCK(device, res = zesDevicePciGetStats(device, &stats));
    if (res == ZE_RESULT_SUCCESS) {
        // Initialize prevReadCounter if not already initialized
        if (prevReadCounter.find(device) == prevReadCounter.end()) {
            prevReadCounter[device] = {0, 0};
        }

        curCounter = (double)stats.rxCounter;
        curTime = (double)stats.timestamp;
        prevCounter = prevReadCounter[device].rxCounter;
        prevTime = prevReadCounter[device].timestamp;
    } else {
        XPUM_LOG_ERROR("{} Failed to get PCIe stats", __FUNCTION__);
        return 0;
    }

    if (curTime > prevTime) {
        // The timestamp is in microseconds
        double deltaTimeInMs = (curTime - prevTime) / MICROSECONDS_TO_SECONDS;
        // The counters are in bytes
        double deltaCounterInKB = 0.0;
        if (curCounter >= prevCounter) {
            deltaCounterInKB = (curCounter - prevCounter) / BYTES_TO_KILOBYTES;
        } else { // Handle counter wraparound
            deltaCounterInKB = ((UINT64_MAX - prevCounter + curCounter + 1) / BYTES_TO_KILOBYTES);
        }
        // throughput is kB/s
        throughput = deltaCounterInKB / deltaTimeInMs;

        // divide by 2 to get the throughput for each direction
        throughput /= 2;
        prevReadCounter[device].rxCounter = stats.rxCounter;
        prevReadCounter[device].timestamp = stats.timestamp;
    }

    return (uint64_t)throughput;
}

uint64_t PCIeManager::getLatestPCIeWriteThroughput(const zes_device_handle_t& device) {
    double prevCounter = 0.0;
    double curCounter = 0.0;
    double curTime = 0.0;
    double prevTime = 0.0;
    double throughput = 0.0;
    zes_pci_stats_t stats = {};
    ze_result_t res = ZE_RESULT_SUCCESS;

    XPUM_ZE_HANDLE_LOCK(device, res = zesDevicePciGetStats(device, &stats));
    if (res == ZE_RESULT_SUCCESS) {
        // Initialize prevWriteCounter if not already initialized
        if (prevWriteCounter.find(device) == prevWriteCounter.end()) {
            prevWriteCounter[device] = {0, 0};
        }

        curCounter = (double)stats.txCounter;
        curTime = (double)stats.timestamp;
        prevCounter = prevWriteCounter[device].txCounter;
        prevTime = prevWriteCounter[device].timestamp;
    } else {
        XPUM_LOG_ERROR("{} Failed to get PCIe stats", __FUNCTION__);
        return 0;
    }

    if (curTime > prevTime) {
        // The timestamp is in microseconds
        double deltaTimeInMs = (curTime - prevTime) / MICROSECONDS_TO_SECONDS;
        // The counters are in bytes
        double deltaCounterInKB = 0.0;
        if (curCounter >= prevCounter) {
            deltaCounterInKB = (curCounter - prevCounter) / BYTES_TO_KILOBYTES;
        } else { // Handle counter wraparound
            deltaCounterInKB = ((UINT64_MAX - prevCounter + curCounter + 1) / BYTES_TO_KILOBYTES);
        }
        // throughput is kB/s
        throughput = deltaCounterInKB / deltaTimeInMs;
        // divide by 2 to get the throughput for each direction
        throughput /= 2;
        prevWriteCounter[device].txCounter = curCounter;
        prevWriteCounter[device].timestamp = curTime;
    }

    return (uint64_t)throughput;
}

uint64_t PCIeManager::getLatestPCIeRead(const zes_device_handle_t& device) {
    uint64_t curCounter = 0;
    zes_pci_stats_t stats = {};
    ze_result_t res = ZE_RESULT_SUCCESS;

    XPUM_ZE_HANDLE_LOCK(device, res = zesDevicePciGetStats(device, &stats));
    if (res == ZE_RESULT_SUCCESS) {
        curCounter = stats.rxCounter;
    }
    return curCounter;
}

uint64_t PCIeManager::getLatestPCIeWrite(const zes_device_handle_t& device) {
    uint64_t curCounter = 0;
    zes_pci_stats_t stats = {};
    ze_result_t res = ZE_RESULT_SUCCESS;

    XPUM_ZE_HANDLE_LOCK(device, res = zesDevicePciGetStats(device, &stats));
    if (res == ZE_RESULT_SUCCESS) {
        curCounter = stats.txCounter;
    }
    return curCounter;
}

} // namespace xpum
