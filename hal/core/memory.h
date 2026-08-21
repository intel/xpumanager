/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _MEMORY_H
#define _MEMORY_H

#include "sysman.h"
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <ze_api.h>
#include <zes_api.h>

/**
 * @brief Display string for memory specification data the platform exposes no source for
 */
inline constexpr std::string_view MEMORY_SPEC_UNKNOWN{"unknown"};

/**
 * @brief Intel sysman extension memory type reported by Crescent Island
 *
 * Mirrors ZES_INTEL_MEM_TYPE_LPDDR5X from hal/core/extensions/zes_intel_gpu_sysman.h.
 * That header is only on the include path in extensions builds, but the value is
 * returned by the sysman driver -- cast into zes_mem_type_t, which has no LPDDR5X
 * enumerator -- no matter how xpu-smi itself was built, so the type name has to be
 * recognized in every build. Kept as a zes_mem_type_t so that the memory type
 * table and its callers never have to cast.
 */
inline constexpr auto MEMORY_INTEL_TYPE_LPDDR5X = static_cast<zes_mem_type_t>(500);

/**
 * @brief Fine-grained video memory specification data
 *
 * Holds the memory specification a device reports about its own video memory.
 * Capacity, bus width and channel count already have their own accessors
 * (getMemorySize(), getMemoryBusWidth(), getMemoryChannels()), so what this adds
 * is the memory form/type plus the identification fields the ticket asks for.
 *
 * The form/type is resolved from two independent Level Zero sources because
 * neither is complete on its own:
 *   - sysman zesMemoryGetProperties(): per memory module, and the only source
 *     that names a product-specific type such as LPDDR5X. Reports
 *     ZES_MEM_TYPE_FORCE_UINT32 on platforms whose product helper has no
 *     mapping (observed on Arrow Lake).
 *   - core zeDeviceGetMemoryProperties() with ze_device_memory_ext_properties_t
 *     chained on pNext. Derived from gtSystemInfo.MemoryType and distinguishes
 *     memory generations that the sysman enum cannot express at all: zes_mem_type_t
 *     has a single ZES_MEM_TYPE_HBM value, while the core enum separates
 *     HBM2/HBM2E/HBM3/HBM3E/HBM4.
 *
 * @note vendor, dateCode and dieInfo have no source in the current software
 *       stack: no Level Zero interface (core, sysman, or the Intel sysman
 *       extensions), no kernel-mode driver interface (sysfs or debugfs), and no
 *       igsc entry point exposes memory manufacturer, date code or IC/die data.
 *       They are carried here so that the collection path and the CLI fields
 *       already exist once a driver or firmware interface does expose them, and
 *       until then they report MEMORY_SPEC_UNKNOWN -- they are never guessed.
 */
struct MemorySpecData
{
	std::string typeName{MEMORY_SPEC_UNKNOWN}; // resolved form/type, e.g. "LPDDR5X", "HBM3"
	std::string vendor{MEMORY_SPEC_UNKNOWN};   // no platform source yet, see note above
	std::string dateCode{MEMORY_SPEC_UNKNOWN}; // no platform source yet, see note above
	std::string dieInfo{MEMORY_SPEC_UNKNOWN};  // no platform source yet, see note above
};

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
	uint32_t memoryModulesCount = 0;
	zes_mem_handle_t *memoryModules = nullptr;

	zes_mem_type_t collectSysmanMemoryType(bool &anySourceAnswered, ze_result_t &lastError);
	static ze_device_memory_ext_type_t collectCoreMemoryType(ze_device_handle_t coreDevice, bool &anySourceAnswered,
															 ze_result_t &lastError);

public:
	memory() = default;
	~memory() override;
	ze_result_t enumMemoryModules(zes_device_handle_t device);
	ze_result_t getProperties(zes_mem_handle_t memhandle, zes_mem_properties_t *properties);
	ze_result_t getState(zes_mem_handle_t memhandle, zes_mem_state_t *state);
	ze_result_t getBandwidth(zes_mem_handle_t memhandle, zes_mem_bandwidth_t *bandwidth);
	ze_result_t getMemorySize(uint64_t *size);
	ze_result_t getMemoryHealth(zes_mem_health_t *health);
	ze_result_t getMemoryChannels(int32_t *channels);
	ze_result_t getMemoryBusWidth(int32_t *busWidth);
	ze_result_t getMemoryUsed(uint64_t *used, double *utilization);
	ze_result_t getMemoryRW(uint64_t *read, uint64_t *write, uint64_t *maxBandwidth, uint64_t *timeStamp);
	ze_result_t getMemoryBandwidthPerTile(std::map<uint32_t, MemoryBandwidthData> &tileBandwidth);
	ze_result_t getMemoryUsagePerTile(std::map<uint32_t, MemoryUsageData> &tileUsage);
	ze_result_t getMemorySpec(ze_device_handle_t coreDevice, MemorySpecData &spec);

	static std::string_view sysmanMemoryTypeToString(zes_mem_type_t type);
	static std::string_view coreMemoryTypeToString(ze_device_memory_ext_type_t type);
	static std::string resolveMemoryTypeName(zes_mem_type_t sysmanType, ze_device_memory_ext_type_t coreType);

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif