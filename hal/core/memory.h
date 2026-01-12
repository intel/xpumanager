/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _MEMORY_H
#define _MEMORY_H

#include "sysman.h"
#include <map>

/**
 * @brief Memory bandwidth snapshot for per-tile tracking
 *
 * Contains read/write counters, max bandwidth, and timestamp for calculating
 * throughput and bandwidth utilization over time.
 */
struct MemoryBandwidthData
{
	uint64_t readCounter = 0;  // bytes read
	uint64_t writeCounter = 0; // bytes written
	uint64_t maxBandwidth = 0; // max bandwidth in bytes/sec
	uint64_t timestamp = 0;	   // timestamp in microseconds
};

/**
 * @brief Memory usage data for per-tile tracking
 *
 * Contains used memory and utilization percentage for a tile.
 */
struct MemoryUsageData
{
	uint64_t usedBytes = 0;			 // used memory in bytes
	double utilizationPercent = 0.0; // utilization as percentage (0-100)
};

class LIBXPUM_API memory : public sysman
{
private:
	uint32_t memoryModulesCount;
	zes_mem_handle_t *memoryModules;

public:
	memory() : memoryModulesCount(0), memoryModules(nullptr) {}
	~memory();
	ze_result_t enumMemoryModules(zes_device_handle_t device);
	ze_result_t getProperties(zes_mem_handle_t memhandle, zes_mem_properties_t *properties);
	ze_result_t getState(zes_mem_handle_t memhandle, zes_mem_state_t *state);
	ze_result_t getBandwidth(zes_mem_handle_t memhandle, zes_mem_bandwidth_t *bandwidth);
	ze_result_t getMemorySize(uint64_t *size);
	ze_result_t getMemoryHealth(zes_mem_health_t *health);
	ze_result_t getMemoryChannels(uint32_t *channels);
	ze_result_t getMemoryBusWidth(uint32_t *busWidth);
	ze_result_t getMemoryUsed(uint64_t *used, double *utilization);
	ze_result_t getMemoryRW(uint64_t *read, uint64_t *write, uint64_t *maxBandwidth, uint64_t *timeStamp);
	ze_result_t getMemoryBandwidthPerTile(std::map<uint32_t, MemoryBandwidthData> &tileBandwidth);
	ze_result_t getMemoryUsagePerTile(std::map<uint32_t, MemoryUsageData> &tileUsage);

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device);
};

#endif