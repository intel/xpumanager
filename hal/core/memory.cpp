/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "memory.h"

#include "sysman.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <new>
#include <utility>

// Only on the include path in extensions builds. Included when it is there so
// that the legacy MEMORY_INTEL_TYPE_LPDDR5X cannot drift from the value the
// Intel sysman extension header defined before LPDDR5X entered the standard
// enum. The legacy value must remain recognized for older CRI drivers.
#if __has_include(<zes_intel_gpu_sysman.h>)
#include <zes_intel_gpu_sysman.h>
static_assert(static_cast<int32_t>(MEMORY_INTEL_TYPE_LPDDR5X) == ZES_INTEL_MEM_TYPE_LPDDR5X,
			  "MEMORY_INTEL_TYPE_LPDDR5X must match the Intel sysman extension value");
#endif

#include <string>
#include <string_view>
#include <vector>
#include <ze_api.h>
#include <zes_api.h>

namespace {
constexpr int32_t CRI_FULL_SKU_MEMORY_MSUS = 20;
constexpr int32_t CRI_SKU4_MEMORY_MSUS = 16;
constexpr int32_t CRI_CHANNELS_PER_MEMORY_MSU = 4;
constexpr int32_t CRI_FULL_SKU_MEMORY_CHANNELS = CRI_FULL_SKU_MEMORY_MSUS * CRI_CHANNELS_PER_MEMORY_MSU;
constexpr int32_t CRI_SKU4_MEMORY_CHANNELS = CRI_SKU4_MEMORY_MSUS * CRI_CHANNELS_PER_MEMORY_MSU;
constexpr uint64_t CRI_SKU4_MEMORY_SIZE = 128ULL * 1024ULL * 1024ULL * 1024ULL;
// Keep synchronized with compute-runtime's criDeviceIds.
constexpr std::array<uint32_t, 5> CRI_DEVICE_IDS{0x674C, 0x674D, 0x674E, 0x674F, 0x6750};

// Sysman can expose allocatable rather than installed capacity, so tolerate
// small firmware/driver reservations. A larger mismatch is not enough evidence
// to override the driver's channel count.
constexpr uint64_t CRI_MEMORY_SIZE_TOLERANCE = 1ULL * 1024ULL * 1024ULL * 1024ULL;

constexpr bool isCriLpddr5xMemoryType(zes_mem_type_t type)
{
	return type == ZES_MEM_TYPE_LPDDR5X || type == MEMORY_INTEL_TYPE_LPDDR5X;
}

constexpr bool isCriDevice(uint32_t pciDeviceId)
{
	return std::find(CRI_DEVICE_IDS.begin(), CRI_DEVICE_IDS.end(), pciDeviceId) != CRI_DEVICE_IDS.end();
}

constexpr bool isCriFullSkuChannelCount(int32_t channels)
{
	// Older CRI drivers reported the MSU count through numChannels. Current
	// drivers report four channels per MSU.
	return channels == CRI_FULL_SKU_MEMORY_MSUS || channels == CRI_FULL_SKU_MEMORY_CHANNELS;
}
} // namespace

/**
 * @brief Enumerates available memory modules for a device
 *
 * This function discovers and catalogs all memory modules available on the
 * specified device. Memory modules represent different memory banks or
 * memory controllers that can be monitored and managed independently.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
ze_result_t memory::enumMemoryModules(zes_device_handle_t device)
{
	memoryModules.clear();
	pciDeviceId = 0;

	uint32_t count = 0;
	ze_result_t result = zesDeviceEnumMemoryModules(device, &count, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to enumerate Memory modules. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}
	if (count == 0) {
		DBG("No memory modules found.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	std::vector<zes_mem_handle_t> modules;
	if (count > modules.max_size()) {
		ERR("Memory module count {} exceeds the supported container size.\n", count);
		return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
	}
	try {
		modules.resize(count);
	} catch (const std::bad_alloc &) {
		ERR("Failed to allocate Memory module handles.\n");
		return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
	}

	const uint32_t capacity = count;
	result = zesDeviceEnumMemoryModules(device, &count, modules.data());
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get Memory modules. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}
	if (count > capacity) {
		ERR("Memory module count grew from {} to {} during enumeration.\n", capacity, count);
		return ZE_RESULT_ERROR_UNKNOWN;
	}
	if (count == 0) {
		DBG("Memory modules disappeared during enumeration.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	modules.resize(count);
	memoryModules = std::move(modules);

	DBG("Found {} memory modules\n", memoryModules.size());
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Retrieves memory properties for a specified memory module
 *
 * This function queries comprehensive properties of a memory module including
 * memory type (HBM, DDR, GDDR), location, physical size, bus width, and
 * channel configuration. It provides detailed memory subsystem information
 * for monitoring and management operations.
 *
 * @param memhandle Handle to the memory module
 * @param properties Pointer to structure that will receive memory properties
 * @return ze_result_t ZE_RESULT_SUCCESS on successful property retrieval, error code otherwise
 */
// Kept an instance method to match the other per-module accessors (getState(),
// getBandwidth()) that callers reach through a memory object.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
ze_result_t memory::getProperties(zes_mem_handle_t memhandle, zes_mem_properties_t *properties)
{
	if (properties == nullptr) {
		ERR("Memory properties output is null\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}

	// Level Zero expects the caller to tag the struct it passes in. Zero
	// initializing leaves stype 0, which is not a zes_structure_type_t value, so
	// tag it here instead of relying on every caller to remember. pNext is
	// [in,out][optional] and may carry a caller-supplied extension struct, so it is
	// left alone; callers own it and are expected to zero-initialize the struct.
	properties->stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;

	const ze_result_t result = zesMemoryGetProperties(memhandle, properties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get Memory properties. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Memory properties retrieved successfully.\n");

	DBG("Memory type: {}\n", sysmanMemoryTypeToString(properties->type));
	DBG("Subdevice ID: {}\n", properties->subdeviceId);
	DBG("Location: ");
	switch (properties->location) {
	case ZES_MEM_LOC_SYSTEM:
		DBG("System\n");
		break;
	case ZES_MEM_LOC_DEVICE:
		DBG("Device\n");
		break;
	default:
		DBG("Unknown\n");
		break;
	}
	DBG("Physical size: {} bytes\n", properties->physicalSize);
	DBG("Bus width: {} bits\n", properties->busWidth);
	DBG("Number of channels: {}\n", properties->numChannels);

	return result;
}

/**
 * @brief Retrieves current memory state and health information
 *
 * This function queries the current state of a memory module including
 * available free memory, total size, and health status. It provides
 * real-time memory utilization and health monitoring capabilities.
 *
 * @param memhandle Handle to the memory module
 * @param state Pointer to structure that will receive memory state information
 * @return ze_result_t ZE_RESULT_SUCCESS on successful state retrieval, error code otherwise
 */
ze_result_t memory::getState(zes_mem_handle_t memhandle, zes_mem_state_t *state)
{
	if (state == nullptr) {
		ERR("Memory state output is null\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}

	// Same contract as getProperties(): stype is an [in] field and zero is not a
	// zes_structure_type_t value, so tag it here. pNext on zes_mem_state_t is
	// [in][optional] and caller-owned, so it is left alone.
	state->stype = ZES_STRUCTURE_TYPE_MEM_STATE;

	const ze_result_t result = zesMemoryGetState(memhandle, state);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get Memory state. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Memory state retrieved successfully.\n");
	DBG("Free memory: {} bytes\n", state->free);
	DBG("Size: {} bytes\n", state->size);
	DBG("Health: ");
	switch (state->health) {
	case ZES_MEM_HEALTH_OK:
		DBG("OK\n");
		break;
	case ZES_MEM_HEALTH_DEGRADED:
		DBG("Degraded\n");
		break;
	case ZES_MEM_HEALTH_CRITICAL:
		DBG("Critical\n");
		break;
	case ZES_MEM_HEALTH_REPLACE:
		DBG("Replace\n");
		break;
	default:
		DBG("Unknown\n");
		break;
	}

	return result;
}

/**
 * @brief Retrieves memory bandwidth utilization statistics
 *
 * This function queries memory bandwidth metrics including read and write
 * counters, maximum theoretical bandwidth, and timestamp information.
 * It provides performance monitoring capabilities for memory subsystem analysis.
 *
 * @param memhandle Handle to the memory module
 * @param bandwidth Pointer to structure that will receive bandwidth statistics
 * @return ze_result_t ZE_RESULT_SUCCESS on successful bandwidth retrieval, error code otherwise
 */
ze_result_t memory::getBandwidth(zes_mem_handle_t memhandle, zes_mem_bandwidth_t *bandwidth)
{
	ze_result_t result = zesMemoryGetBandwidth(memhandle, bandwidth);
	if (result != ZE_RESULT_SUCCESS) {
		DBG("Failed to get Memory bandwidth. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Memory bandwidth retrieved successfully.\n");
	DBG("Read counter: {} bytes\n", bandwidth->readCounter);
	DBG("Write counter: {} bytes\n", bandwidth->writeCounter);
	DBG("Max bandwidth: {} bytes/sec\n", bandwidth->maxBandwidth);
	DBG("Timestamp: {} ns\n", bandwidth->timestamp);

	return result;
}

/**
 * @brief Calculates total memory size across all memory modules
 *
 * This function aggregates the total memory size from all available memory
 * modules on the device. It provides comprehensive memory capacity information
 * for system monitoring and resource management operations.
 *
 * @param size Pointer to variable that will receive total memory size in bytes
 * @return ze_result_t ZE_RESULT_SUCCESS on successful size calculation, error code otherwise
 */
ze_result_t memory::getMemorySize(uint64_t *size)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_mem_state_t state = {};

	if (size == nullptr) {
		ERR("Size pointer is null.\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}
	*size = 0;

	for (std::size_t i = 0; i < memoryModules.size(); i++) {
		result = getState(memoryModules[i], &state);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory state for module {}. 0x{:X} ({})\n", i, result, l0_error_to_string(result));
			return result;
		}
		*size += state.size;
	}
	return result;
}

/**
 * @brief Determines overall memory health status across all modules
 *
 * This function evaluates the health status of all memory modules and returns
 * the most severe health condition found. It prioritizes critical/replace status
 * over degraded, and degraded over OK, providing comprehensive memory health assessment.
 *
 * @param health Pointer to variable that will receive overall memory health status
 * @return ze_result_t ZE_RESULT_SUCCESS on successful health assessment, error code otherwise
 */
ze_result_t memory::getMemoryHealth(zes_mem_health_t *health)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_mem_state_t state = {};

	if (health == nullptr) {
		ERR("Health pointer is null.\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}
	*health = ZES_MEM_HEALTH_UNKNOWN;

	for (std::size_t i = 0; i < memoryModules.size(); i++) {
		result = getState(memoryModules[i], &state);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory state for module {}. 0x{:X} ({})\n", i, result, l0_error_to_string(result));
			return result;
		}
		if (state.health == ZES_MEM_HEALTH_CRITICAL || state.health == ZES_MEM_HEALTH_REPLACE) {
			*health = state.health;
			DBG("Memory health is critical or replace for module {}.\n", i);
			return result;
		} else if (state.health == ZES_MEM_HEALTH_DEGRADED) {
			*health = ZES_MEM_HEALTH_DEGRADED;
			DBG("Memory health is degraded for module {}.\n", i);
			return result;
		} else if (state.health == ZES_MEM_HEALTH_OK) {
			// Only in this case we continue the for loop because the health of this module is OK
			// and we want to check the health of other modules.
			// If all modules are OK, we will return ZES_MEM_HEALTH_OK at the end.
			// If any module is degraded, critical, or replace, we will return that health status.
			// This is to ensure that we do not prematurely return ZES_MEM_HEALTH_OK
			// if there are other modules that might have a different health status.
			// This is a design choice to ensure we report the most severe health status
			// across all memory modules.
			*health = ZES_MEM_HEALTH_OK;
			DBG("Memory health is OK for module {}.\n", i);
		} else {
			*health = ZES_MEM_HEALTH_UNKNOWN;
			DBG("Memory health is unknown for module {}.\n", i);
			return result;
		}
	}
	return result;
}

/**
 * @brief Corrects the CRI soft-SKU memory channel count when capacity proves fewer channels are populated
 *
 * Older compute-runtime versions report CRI's 20-MSU full-SKU maximum through
 * numChannels; current versions report 80 channels (four per MSU). For the
 * 128 GiB SKU4, the corresponding values are 16 and 64. Capacity independently
 * identifies the affected SKU. Every other type, count, and capacity is kept.
 *
 * @param pciDeviceId PCI device ID reported by Sysman
 * @param type Memory type reported by Sysman
 * @param location Memory location reported by Sysman
 * @param physicalSizeBytes Installed or allocatable memory capacity in bytes
 * @param reportedChannels Channel count reported by Sysman
 * @return Corrected CRI channel count, or reportedChannels when no correction is justified
 */
int32_t memory::normalizeCriMemoryChannelCount(uint32_t pciDeviceId, zes_mem_type_t type, zes_mem_loc_t location,
											   uint64_t physicalSizeBytes, int32_t reportedChannels)
{
	if (!isCriDevice(pciDeviceId) || !isCriLpddr5xMemoryType(type) || location != ZES_MEM_LOC_DEVICE ||
		!isCriFullSkuChannelCount(reportedChannels)) {
		return reportedChannels;
	}

	const uint64_t sizeDifference =
		std::max(physicalSizeBytes, CRI_SKU4_MEMORY_SIZE) - std::min(physicalSizeBytes, CRI_SKU4_MEMORY_SIZE);
	if (sizeDifference > CRI_MEMORY_SIZE_TOLERANCE) {
		return reportedChannels;
	}

	return reportedChannels == CRI_FULL_SKU_MEMORY_MSUS ? CRI_SKU4_MEMORY_MSUS : CRI_SKU4_MEMORY_CHANNELS;
}

/**
 * @brief Retrieves memory channel configuration information
 *
 * This function queries the number of memory channels available across
 * memory modules. Memory channels represent parallel data paths that
 * affect memory bandwidth and performance characteristics.
 *
 * @param channels Pointer to variable that will receive number of memory channels.
 *                 Set to -1 by the driver when the number of channels is unknown
 * @return ze_result_t ZE_RESULT_SUCCESS on successful channel retrieval, error code otherwise
 */
ze_result_t memory::getMemoryChannels(int32_t *channels)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_mem_properties_t properties = {};

	if (channels == nullptr) {
		ERR("Channels pointer is null.\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}
	*channels = 0;
	if (memoryModules.empty()) {
		DBG("No memory modules available for channel reporting.\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	int32_t resolvedChannels = 0;
	for (std::size_t i = 0; i < memoryModules.size(); i++) {
		result = getProperties(memoryModules[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory properties for module {}. 0x{:X} ({})\n", i, result, l0_error_to_string(result));
			return result;
		}

		uint64_t physicalSize = properties.physicalSize;
		if (isCriDevice(pciDeviceId) && isCriLpddr5xMemoryType(properties.type) &&
			properties.location == ZES_MEM_LOC_DEVICE && isCriFullSkuChannelCount(properties.numChannels) &&
			physicalSize == 0) {
			zes_mem_state_t state = {};
			const ze_result_t stateResult = getState(memoryModules[i], &state);
			if (stateResult == ZE_RESULT_SUCCESS) {
				physicalSize = state.size;
			} else {
				DBG("Memory size unavailable for CRI channel correction on module {}. 0x{:X} ({})\n", i, stateResult,
					l0_error_to_string(stateResult));
			}
		}

		const int32_t normalizedChannels = normalizeCriMemoryChannelCount(
			pciDeviceId, properties.type, properties.location, physicalSize, properties.numChannels);
		if (normalizedChannels != properties.numChannels) {
			DBG("Corrected CRI memory channels for module {} from {} to {} using capacity {} bytes\n", i,
				properties.numChannels, normalizedChannels, physicalSize);
		}
		DBG("Memory properties for module {}: numChannels={}\n", i, normalizedChannels);
		resolvedChannels = normalizedChannels;
	}
	*channels = resolvedChannels;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Retrieves memory bus width configuration
 *
 * This function queries the memory bus width in bits across memory modules.
 * Bus width determines the amount of data that can be transferred per clock
 * cycle and directly affects memory bandwidth performance.
 *
 * @param busWidth Pointer to variable that will receive memory bus width in bits.
 *                 Set to -1 by the driver when the bus width is unknown
 * @return ze_result_t ZE_RESULT_SUCCESS on successful bus width retrieval, error code otherwise
 */
ze_result_t memory::getMemoryBusWidth(int32_t *busWidth)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_mem_properties_t properties = {};

	if (busWidth == nullptr) {
		ERR("Bus width pointer is null.\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}
	*busWidth = 0;

	for (std::size_t i = 0; i < memoryModules.size(); i++) {
		result = getProperties(memoryModules[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory properties for module {}. 0x{:X} ({})\n", i, result, l0_error_to_string(result));
			return result;
		}
		DBG("Memory properties for module {}: busWidth={}\n", i, properties.busWidth);
		*busWidth = properties.busWidth;
	}
	return result;
}

/**
 * @brief Calculates memory usage statistics and utilization percentage
 *
 * This function determines current memory usage by calculating the difference
 * between total and free memory across all modules. It aggregates used memory
 * across all modules and computes device-wide utilization percentage.
 *
 * @param used Pointer to variable that will receive aggregated used memory in bytes (can be null)
 * @param utilization Pointer to variable that will receive device-wide utilization percentage (can be null)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful usage calculation, error code otherwise
 */
ze_result_t memory::getMemoryUsed(uint64_t *used, double *utilization)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	uint64_t totalUsedBytes = 0;
	uint64_t totalSizeBytes = 0;

	if (used != nullptr) {
		*used = 0;
	}

	if (utilization != nullptr) {
		*utilization = 0;
	}

	for (std::size_t i = 0; i < memoryModules.size(); i++) {
		zes_mem_properties_t properties = {};
		zes_mem_state_t state = {};

		result = getProperties(memoryModules[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory properties for module {}. 0x{:X} ({})\n", i, result, l0_error_to_string(result));
			return result;
		}
		result = getState(memoryModules[i], &state);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory state for module {}. 0x{:X} ({})\n", i, result, l0_error_to_string(result));
			return result;
		}

		uint64_t moduleTotal = (properties.physicalSize == 0) ? state.size : properties.physicalSize;
		uint64_t moduleUsed = (moduleTotal >= state.free) ? (moduleTotal - state.free) : 0;

		totalUsedBytes += moduleUsed;
		totalSizeBytes += moduleTotal;
	}

	if (used != nullptr) {
		*used = totalUsedBytes;
	}

	if (utilization != nullptr && totalSizeBytes > 0) {
		*utilization = (double)totalUsedBytes * 100.0 / (double)totalSizeBytes;
	}

	return result;
}

/**
 * @brief Retrieves memory read/write statistics and bandwidth information
 *
 * This function aggregates memory bandwidth metrics across all memory modules
 * including read and write counters, maximum bandwidth capability, and timing
 * information for comprehensive memory performance analysis.
 *
 * @param read Pointer to variable that will receive total read counter (can be null)
 * @param write Pointer to variable that will receive total write counter (can be null)
 * @param maxBandwidth Pointer to variable that will receive maximum bandwidth (can be null)
 * @param timeStamp Pointer to variable that will receive timestamp (can be null)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful bandwidth retrieval, error code otherwise
 */
ze_result_t memory::getMemoryRW(uint64_t *read, uint64_t *write, uint64_t *maxBandwidth, uint64_t *timeStamp)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_mem_bandwidth_t bandwidth;

	if (read) {
		*read = 0;
	}

	if (write) {
		*write = 0;
	}

	if (maxBandwidth) {
		*maxBandwidth = 0;
	}

	if (timeStamp) {
		*timeStamp = 0;
	}

	for (std::size_t i = 0; i < memoryModules.size(); i++) {

		result = getBandwidth(memoryModules[i], &bandwidth);
		if (result != ZE_RESULT_SUCCESS) {
			DBG("Failed to get Memory bandwidth for module {}. 0x{:X} ({})\n", i, result, l0_error_to_string(result));
			return result;
		}

		if (read) {
			*read += bandwidth.readCounter;
		}

		if (write) {
			*write += bandwidth.writeCounter;
		}

		if (timeStamp) {
			*timeStamp = bandwidth.timestamp;
		}

		if (maxBandwidth) {
			*maxBandwidth = bandwidth.maxBandwidth;
		}
	}
	return result;
}

/**
 * @brief Gets memory bandwidth counters and properties per tile
 *
 * This function retrieves memory read/write counters, maximum bandwidth, and
 * timestamp for each tile's device memory. Only device memory (ZES_MEM_LOC_DEVICE)
 * is included. For tiles with multiple memory modules, values are summed.
 *
 * @param [out] tileBandwidth Output map of tile ID to MemoryBandwidthData
 * @return ze_result_t ZE_RESULT_SUCCESS on successful retrieval, error code otherwise
 */
ze_result_t memory::getMemoryBandwidthPerTile(std::map<uint32_t, MemoryBandwidthData> &tileBandwidth)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;

	tileBandwidth.clear();

	for (std::size_t i = 0; i < memoryModules.size(); i++) {
		zes_mem_properties_t properties = {};
		result = getProperties(memoryModules[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory properties for module {}. 0x{:X} ({})\n", i, result, l0_error_to_string(result));
			continue;
		}

		if (properties.location != ZES_MEM_LOC_DEVICE) {
			continue;
		}

		zes_mem_bandwidth_t bandwidth = {};
		result = getBandwidth(memoryModules[i], &bandwidth);
		if (result != ZE_RESULT_SUCCESS) {
			DBG("Failed to get Memory bandwidth for module {}. 0x{:X} ({})\n", i, result, l0_error_to_string(result));
			continue;
		}

		uint32_t tileId = properties.onSubdevice ? properties.subdeviceId : 0;

		if (tileBandwidth.find(tileId) == tileBandwidth.end()) {
			tileBandwidth[tileId] = {.readCounter = 0, .writeCounter = 0, .maxBandwidth = 0, .timestamp = 0};
		}

		tileBandwidth[tileId].readCounter += bandwidth.readCounter;
		tileBandwidth[tileId].writeCounter += bandwidth.writeCounter;
		tileBandwidth[tileId].maxBandwidth += bandwidth.maxBandwidth;
		// Use the latest (maximum) timestamp across all modules so that any rate calculation
		// uses a time window that includes all accumulated counter values from every module,
		// avoiding premature or inconsistent bandwidth rate computations based on earlier samples.
		if (bandwidth.timestamp > tileBandwidth[tileId].timestamp) {
			tileBandwidth[tileId].timestamp = bandwidth.timestamp;
		}
	}

	return result;
}

/**
 * @brief Gets memory used and utilization percentage per tile
 *
 * This function retrieves the amount of memory used (in bytes) and the memory
 * utilization percentage for each tile's device memory. Only device memory
 * (ZES_MEM_LOC_DEVICE) is included. For tiles with multiple memory modules,
 * values are summed and utilization is calculated from totals.
 *
 * @param [out] tileUsage Output map of tile ID to MemoryUsageData (usedBytes, utilizationPercent)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful retrieval, error code otherwise
 */
ze_result_t memory::getMemoryUsagePerTile(std::map<uint32_t, MemoryUsageData> &tileUsage)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;

	tileUsage.clear();

	std::map<uint32_t, uint64_t> tileUsed;
	std::map<uint32_t, uint64_t> tileTotal;

	for (std::size_t i = 0; i < memoryModules.size(); i++) {
		zes_mem_properties_t properties = {};
		result = getProperties(memoryModules[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory properties for module {}. 0x{:X} ({})\n", i, result, l0_error_to_string(result));
			continue;
		}

		if (properties.location != ZES_MEM_LOC_DEVICE) {
			continue;
		}

		zes_mem_state_t state = {};
		result = getState(memoryModules[i], &state);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory state for module {}. 0x{:X} ({})\n", i, result, l0_error_to_string(result));
			continue;
		}

		uint32_t tileId = properties.onSubdevice ? properties.subdeviceId : 0;

		uint64_t used = state.size - state.free;

		// Prefer physicalSize (hardware capacity) over state.size (allocatable memory) for accurate utilization.
		// physicalSize may be 0 on older drivers or unsupported hardware, in which case fall back to state.size.
		uint64_t total = (properties.physicalSize > 0) ? properties.physicalSize : state.size;

		if (tileUsed.find(tileId) == tileUsed.end()) {
			tileUsed[tileId] = 0;
			tileTotal[tileId] = 0;
		}

		tileUsed[tileId] += used;
		tileTotal[tileId] += total;
	}

	for (const auto &pair : tileUsed) {
		uint32_t tileId = pair.first;
		uint64_t used = pair.second;
		uint64_t total = tileTotal[tileId];

		double utilization = 0.0;
		if (total > 0) {
			utilization = (static_cast<double>(used) / static_cast<double>(total)) * 100.0;
		}

		tileUsage[tileId] = {.usedBytes = used, .utilizationPercent = utilization};
	}

	return result;
}

/**
 * @brief Gets memory used and utilization percentage for system memory
 *
 * This function retrieves the amount of memory used (in bytes) and the memory
 * utilization percentage for a device's system memory. Only system memory
 * (ZES_MEM_LOC_SYSTEM) is included. Since only iGPUs report system memory, and
 * iGPUs are always single-tile, results are reported as a single tile-0 entry
 * rather than per tile.
 *
 * @param [out] tileUsage Output map with a single tile-0 entry (usedBytes, utilizationPercent)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful retrieval, error code otherwise
 */
ze_result_t memory::getSystemMemoryUsage(std::map<uint32_t, MemoryUsageData> &tileUsage)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;

	tileUsage.clear();

	uint64_t totalUsed = 0;
	uint64_t totalCapacity = 0;
	bool foundSystemMemory = false;

	for (uint32_t i = 0; i < memoryModulesCount; i++) {
		zes_mem_properties_t properties = {};
		result = getProperties(memoryModules[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory properties for module {}. 0x{:X} ({})\n", i, result, l0_error_to_string(result));
			continue;
		}

		if (properties.location != ZES_MEM_LOC_SYSTEM) {
			continue;
		}

		zes_mem_state_t state = {};
		result = getState(memoryModules[i], &state);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory state for module {}. 0x{:X} ({})\n", i, result, l0_error_to_string(result));
			continue;
		}

		foundSystemMemory = true;

		// Guard against underflow if a driver ever reports free > size (matches getMemoryUsed()).
		totalUsed += (state.size >= state.free) ? (state.size - state.free) : 0;

		// Prefer physicalSize (hardware capacity) over state.size (allocatable memory) for accurate utilization.
		// physicalSize may be 0 on older drivers or unsupported hardware, in which case fall back to state.size.
		totalCapacity += (properties.physicalSize > 0) ? properties.physicalSize : state.size;
	}

	if (foundSystemMemory) {
		double utilization = 0.0;
		if (totalCapacity > 0) {
			utilization = (static_cast<double>(totalUsed) / static_cast<double>(totalCapacity)) * 100.0;
		}
		tileUsage[0] = {.usedBytes = totalUsed, .utilizationPercent = utilization};
	}

	return result;
}

namespace {
/**
 * @brief One memory type name and the value each Level Zero source uses for it
 *
 * Both enums use their FORCE_UINT32 value as "no type", so it doubles as the
 * marker for a name the source cannot express. The legacy Intel LPDDR5X value
 * predates the standard LPDDR5X enumerators and therefore has no core partner.
 */
struct MemoryTypeName
{
	zes_mem_type_t sysmanType;
	ze_device_memory_ext_type_t coreType;
	std::string_view name;
};

constexpr zes_mem_type_t NO_SYSMAN_TYPE = ZES_MEM_TYPE_FORCE_UINT32;
constexpr ze_device_memory_ext_type_t NO_CORE_TYPE = ZE_DEVICE_MEMORY_EXT_TYPE_FORCE_UINT32;

/**
 * @brief The memory types both Level Zero sources can report, paired by name
 *
 * A table rather than a switch per source so that the two enums stay visibly
 * aligned and a new memory generation is one row instead of two case labels.
 * The standard LPDDR5X values are paired normally. The legacy Intel sysman
 * value remains a separate row for compatibility with older CRI drivers.
 */
constexpr std::array MEMORY_TYPE_NAMES{
	MemoryTypeName{ZES_MEM_TYPE_HBM, ZE_DEVICE_MEMORY_EXT_TYPE_HBM, "HBM"},
	MemoryTypeName{NO_SYSMAN_TYPE, ZE_DEVICE_MEMORY_EXT_TYPE_HBM2, "HBM2"},
	MemoryTypeName{NO_SYSMAN_TYPE, ZE_DEVICE_MEMORY_EXT_TYPE_HBM2E, "HBM2E"},
	MemoryTypeName{NO_SYSMAN_TYPE, ZE_DEVICE_MEMORY_EXT_TYPE_HBM3, "HBM3"},
	MemoryTypeName{NO_SYSMAN_TYPE, ZE_DEVICE_MEMORY_EXT_TYPE_HBM3E, "HBM3E"},
	MemoryTypeName{NO_SYSMAN_TYPE, ZE_DEVICE_MEMORY_EXT_TYPE_HBM4, "HBM4"},
	MemoryTypeName{ZES_MEM_TYPE_DDR, ZE_DEVICE_MEMORY_EXT_TYPE_DDR, "DDR"},
	MemoryTypeName{NO_SYSMAN_TYPE, ZE_DEVICE_MEMORY_EXT_TYPE_DDR2, "DDR2"},
	MemoryTypeName{ZES_MEM_TYPE_DDR3, ZE_DEVICE_MEMORY_EXT_TYPE_DDR3, "DDR3"},
	MemoryTypeName{ZES_MEM_TYPE_DDR4, ZE_DEVICE_MEMORY_EXT_TYPE_DDR4, "DDR4"},
	MemoryTypeName{ZES_MEM_TYPE_DDR5, ZE_DEVICE_MEMORY_EXT_TYPE_DDR5, "DDR5"},
	MemoryTypeName{ZES_MEM_TYPE_LPDDR, ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR, "LPDDR"},
	MemoryTypeName{ZES_MEM_TYPE_LPDDR3, ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR3, "LPDDR3"},
	MemoryTypeName{ZES_MEM_TYPE_LPDDR4, ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR4, "LPDDR4"},
	MemoryTypeName{ZES_MEM_TYPE_LPDDR5, ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR5, "LPDDR5"},
	MemoryTypeName{ZES_MEM_TYPE_LPDDR5X, ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR5X, "LPDDR5X"},
	MemoryTypeName{MEMORY_INTEL_TYPE_LPDDR5X, NO_CORE_TYPE, "LPDDR5X"},
	MemoryTypeName{ZES_MEM_TYPE_SRAM, ZE_DEVICE_MEMORY_EXT_TYPE_SRAM, "SRAM"},
	MemoryTypeName{ZES_MEM_TYPE_L1, ZE_DEVICE_MEMORY_EXT_TYPE_L1, "L1"},
	MemoryTypeName{ZES_MEM_TYPE_L3, ZE_DEVICE_MEMORY_EXT_TYPE_L3, "L3"},
	MemoryTypeName{ZES_MEM_TYPE_GRF, ZE_DEVICE_MEMORY_EXT_TYPE_GRF, "GRF"},
	MemoryTypeName{ZES_MEM_TYPE_SLM, ZE_DEVICE_MEMORY_EXT_TYPE_SLM, "SLM"},
	MemoryTypeName{ZES_MEM_TYPE_GDDR4, ZE_DEVICE_MEMORY_EXT_TYPE_GDDR4, "GDDR4"},
	MemoryTypeName{ZES_MEM_TYPE_GDDR5, ZE_DEVICE_MEMORY_EXT_TYPE_GDDR5, "GDDR5"},
	MemoryTypeName{ZES_MEM_TYPE_GDDR5X, ZE_DEVICE_MEMORY_EXT_TYPE_GDDR5X, "GDDR5X"},
	MemoryTypeName{ZES_MEM_TYPE_GDDR6, ZE_DEVICE_MEMORY_EXT_TYPE_GDDR6, "GDDR6"},
	MemoryTypeName{ZES_MEM_TYPE_GDDR6X, ZE_DEVICE_MEMORY_EXT_TYPE_GDDR6X, "GDDR6X"},
	MemoryTypeName{ZES_MEM_TYPE_GDDR7, ZE_DEVICE_MEMORY_EXT_TYPE_GDDR7, "GDDR7"},
};
} // namespace

/**
 * @brief Converts a sysman memory type to its display string
 *
 * @param type Memory type reported by zesMemoryGetProperties()
 * @return std::string_view JEDEC name of the memory type, or MEMORY_SPEC_UNKNOWN when
 *         the type is not a value this Level Zero version defines (the driver
 *         reports ZES_MEM_TYPE_FORCE_UINT32 when it has no mapping)
 */
std::string_view memory::sysmanMemoryTypeToString(zes_mem_type_t type)
{
	// Checked first so that "no type" never matches the rows using the same
	// value to mean "the sysman enum cannot express this name".
	if (type == NO_SYSMAN_TYPE) {
		return MEMORY_SPEC_UNKNOWN;
	}

	for (const MemoryTypeName &entry : MEMORY_TYPE_NAMES) {
		if (entry.sysmanType == type) {
			return entry.name;
		}
	}

	return MEMORY_SPEC_UNKNOWN;
}

/**
 * @brief Converts a core memory extension type to its display string
 *
 * The core enum covers memory generations the sysman enum does not (HBM2/HBM2E/
 * HBM3/HBM3E/HBM4), which is why it is consulted when sysman reports no type.
 *
 * @param type Memory type reported through ze_device_memory_ext_properties_t
 * @return std::string_view JEDEC name of the memory type, or MEMORY_SPEC_UNKNOWN when
 *         the type is not a value this Level Zero version defines
 */
std::string_view memory::coreMemoryTypeToString(ze_device_memory_ext_type_t type)
{
	if (type == NO_CORE_TYPE) {
		return MEMORY_SPEC_UNKNOWN;
	}

	for (const MemoryTypeName &entry : MEMORY_TYPE_NAMES) {
		if (entry.coreType == type) {
			return entry.name;
		}
	}

	return MEMORY_SPEC_UNKNOWN;
}

/**
 * @brief Picks the memory form/type to report from the two Level Zero sources
 *
 * When only one source names a type, that name is used. When both do, the more
 * specific name wins if one is a refinement of the other: zes_mem_type_t has a
 * single HBM value, so sysman answers "HBM" for an HBM3 part while the core
 * extension answers "HBM3". Legacy drivers can likewise expose a more specific
 * LPDDR5X sysman extension value while their core mapping only knows LPDDR5. If
 * the two name unrelated families the sysman answer is kept, because it is the
 * per-product, per-module value while the core type is a coarser mapping of the
 * same hardware-config data. If neither source names a type,
 * MEMORY_SPEC_UNKNOWN is returned rather than a guess.
 *
 * @param sysmanType Type from zesMemoryGetProperties()
 * @param coreType Type from ze_device_memory_ext_properties_t
 * @return std::string Memory type display name, or MEMORY_SPEC_UNKNOWN
 */
std::string memory::resolveMemoryTypeName(zes_mem_type_t sysmanType, ze_device_memory_ext_type_t coreType)
{
	const std::string_view fromSysman = sysmanMemoryTypeToString(sysmanType);
	const std::string_view fromCore = coreMemoryTypeToString(coreType);

	if (fromSysman == MEMORY_SPEC_UNKNOWN) {
		// Also covers "neither source knows": fromCore is MEMORY_SPEC_UNKNOWN then.
		return std::string(fromCore);
	}
	if (fromCore == MEMORY_SPEC_UNKNOWN) {
		return std::string(fromSysman);
	}

	if (fromCore.size() > fromSysman.size() && fromCore.starts_with(fromSysman)) {
		return std::string(fromCore);
	}
	if (fromSysman.size() > fromCore.size() && fromSysman.starts_with(fromCore)) {
		return std::string(fromSysman);
	}
	if (fromSysman != fromCore) {
		DBG("Memory type sources disagree (sysman {}, core {}); reporting the sysman value\n", fromSysman, fromCore);
	}
	return std::string(fromSysman);
}

/**
 * @brief Determines the memory type the sysman layer reports for the video memory
 *
 * Ranks the enumerated modules so a device-local one always wins over a system
 * one: an integrated GPU reports its memory as ZES_MEM_LOC_SYSTEM and is the only
 * case where a system module supplies the answer.
 *
 * @param[in,out] anySourceAnswered Set to true when at least one module answered
 * @param[in,out] lastError Set to the last error a module returned, if any
 * @return zes_mem_type_t Reported type, or ZES_MEM_TYPE_FORCE_UINT32 when unknown
 */
zes_mem_type_t memory::collectSysmanMemoryType(bool &anySourceAnswered, ze_result_t &lastError)
{
	constexpr int rankNone = 0;
	constexpr int rankSystem = 1;
	constexpr int rankDeviceLocal = 2;

	zes_mem_type_t sysmanType = ZES_MEM_TYPE_FORCE_UINT32;
	int bestRank = rankNone;

	for (zes_mem_handle_t handle : memoryModules) {
		zes_mem_properties_t properties = {};
		if (const ze_result_t result = getProperties(handle, &properties); result != ZE_RESULT_SUCCESS) {
			lastError = result;
			continue;
		}
		anySourceAnswered = true;

		const int rank = (properties.location == ZES_MEM_LOC_DEVICE) ? rankDeviceLocal : rankSystem;
		if (rank < bestRank) {
			continue;
		}
		if (rank > bestRank) {
			bestRank = rank;
			sysmanType = properties.type;
			continue;
		}
		// Same rank: only fill in a type the earlier module of this rank lacked.
		if (sysmanType == ZES_MEM_TYPE_FORCE_UINT32) {
			sysmanType = properties.type;
		} else if (properties.type != sysmanType && properties.type != ZES_MEM_TYPE_FORCE_UINT32) {
			DBG("Same-rank memory modules disagree on memory type ({} vs {}); keeping the first\n",
				sysmanMemoryTypeToString(sysmanType), sysmanMemoryTypeToString(properties.type));
		}
	}

	return sysmanType;
}

/**
 * @brief Determines the memory type the core memory extension reports
 *
 * The extension struct is chained on the pNext of every memory group and is
 * preset to its sentinel, because a driver may ignore an extension it does not
 * implement and still return success: zero-initialized memory reads back as
 * ZE_DEVICE_MEMORY_EXT_TYPE_HBM (value 0) and would be reported as a real type.
 *
 * @param[in] coreDevice Core (non-sysman) Level Zero device handle
 * @param[in,out] anySourceAnswered Set to true when the device answered
 * @param[in,out] lastError Set to the error the device returned, if any
 * @return ze_device_memory_ext_type_t Reported type, or
 *         ZE_DEVICE_MEMORY_EXT_TYPE_FORCE_UINT32 when unknown
 */
ze_device_memory_ext_type_t memory::collectCoreMemoryType(ze_device_handle_t coreDevice, bool &anySourceAnswered,
														  ze_result_t &lastError)
{
	ze_device_memory_ext_type_t coreType = ZE_DEVICE_MEMORY_EXT_TYPE_FORCE_UINT32;

	uint32_t count = 0;
	ze_result_t result = zeDeviceGetMemoryProperties(coreDevice, &count, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		DBG("Failed to count core memory properties. 0x{:X} ({})\n", result, l0_error_to_string(result));
		lastError = result;
		return coreType;
	}
	if (count == 0) {
		DBG("Device reports no core memory properties\n");
		return coreType;
	}

	std::vector<ze_device_memory_properties_t> props(count);
	std::vector<ze_device_memory_ext_properties_t> ext(count);
	for (uint32_t i = 0; i < count; i++) {
		props[i].stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_PROPERTIES;
		ext[i].stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_EXT_PROPERTIES;
		props[i].pNext = &ext[i];
		ext[i].type = ZE_DEVICE_MEMORY_EXT_TYPE_FORCE_UINT32;
	}

	const uint32_t capacity = count;
	result = zeDeviceGetMemoryProperties(coreDevice, &count, props.data());
	if (result != ZE_RESULT_SUCCESS) {
		DBG("Failed to get core memory properties. 0x{:X} ({})\n", result, l0_error_to_string(result));
		lastError = result;
		return coreType;
	}

	anySourceAnswered = true;
	// The driver may report fewer groups on the second call; it can never fill
	// more entries than the array it was handed.
	for (uint32_t i = 0; i < count && i < capacity; i++) {
		DBG("Core memory group {}: name={} totalSize={} extType={} physicalSize={}\n", i, props[i].name,
			props[i].totalSize, coreMemoryTypeToString(ext[i].type), ext[i].physicalSize);

		if (coreType == ZE_DEVICE_MEMORY_EXT_TYPE_FORCE_UINT32) {
			coreType = ext[i].type;
		}
	}

	return coreType;
}

/**
 * @brief Collects the fine-grained video memory specification of a device
 *
 * Resolves the memory form/type from the sysman per-module properties (already
 * enumerated by init()) and from the core device memory properties with their
 * pNext extension, so the type is reported whenever either source knows it. The
 * identification fields no interface exposes are left as MEMORY_SPEC_UNKNOWN.
 *
 * The core query is issued here rather than reusing the cached
 * device::getMemProps() array because an extension struct chained on pNext
 * cannot survive the memcpy into that array -- the cached copy would hold a
 * pointer to a stack temporary.
 *
 * @param[in] coreDevice Core (non-sysman) Level Zero device handle; may be null
 *                       on a device in survivability mode, in which case only
 *                       the sysman type is collected
 * @param[out] spec Memory specification to populate; left at its defaults
 *                  (everything MEMORY_SPEC_UNKNOWN) when no source answers
 * @return ze_result_t ZE_RESULT_SUCCESS when at least one source answered, the
 *         last error encountered when every source failed, or
 *         ZE_RESULT_ERROR_UNSUPPORTED_FEATURE when there was nothing to query
 */
ze_result_t memory::getMemorySpec(ze_device_handle_t coreDevice, MemorySpecData &spec)
{
	TRACING(); // NOLINT(misc-const-correctness)

	spec = MemorySpecData{};

	bool anySourceAnswered = false;
	ze_result_t lastError = ZE_RESULT_SUCCESS;

	const zes_mem_type_t sysmanType = collectSysmanMemoryType(anySourceAnswered, lastError);

	ze_device_memory_ext_type_t coreType = ZE_DEVICE_MEMORY_EXT_TYPE_FORCE_UINT32;
	if (coreDevice != nullptr) {
		coreType = collectCoreMemoryType(coreDevice, anySourceAnswered, lastError);
	} else {
		DBG("No core device handle; collecting the sysman memory type only\n");
	}

	spec.typeName = resolveMemoryTypeName(sysmanType, coreType);

	if (!anySourceAnswered) {
		if (lastError != ZE_RESULT_SUCCESS) {
			ERR("Failed to collect memory specification. 0x{:X} ({})\n", lastError, l0_error_to_string(lastError));
			return lastError;
		}
		DBG("No memory module and no core memory group to read the memory specification from\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	DBG("Memory specification: type={} (sysman {}, core {})\n", spec.typeName, sysmanMemoryTypeToString(sysmanType),
		coreMemoryTypeToString(coreType));

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Initializes the memory management subsystem
 *
 * This function enumerates all available memory modules and caches the PCI
 * device ID used to scope product-specific corrections.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t memory::init(zes_device_handle_t device)
{
	TRACING();

	const ze_result_t result = enumMemoryModules(device);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	zes_device_properties_t properties = {};
	properties.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
	const ze_result_t propertiesResult = zesDeviceGetProperties(device, &properties);
	if (propertiesResult == ZE_RESULT_SUCCESS) {
		pciDeviceId = properties.core.deviceId;
	} else {
		DBG("Device ID unavailable; CRI memory channel correction is disabled. 0x{:X} ({})\n", propertiesResult,
			l0_error_to_string(propertiesResult));
	}

	return result;
}

/**
 * @brief Executes comprehensive memory system diagnostics and data collection
 *
 * This function performs a complete memory system scan by retrieving properties,
 * state, and bandwidth information for all memory modules. It serves as a
 * diagnostic routine for memory subsystem health and performance assessment.
 *
 * This is a best-effort sweep: each per-module query logs its own failure and the scan
 * continues, so one unreadable module does not hide the remaining ones. Bandwidth in
 * particular is unsupported on many parts.
 *
 * @param device Handle to the Level Zero Sysman device (unused in current implementation)
 * @return ze_result_t Always ZE_RESULT_SUCCESS; individual query failures are logged, not returned
 */
ze_result_t memory::zesRun(UNUSED zes_device_handle_t device)
{
	zes_mem_state_t state = {};
	zes_mem_properties_t properties = {};
	zes_mem_bandwidth_t bandwidth = {};

	for (std::size_t i = 0; i < memoryModules.size(); i++) {
		// Results deliberately discarded: each callee logs its own failure and this sweep is
		// diagnostic only. The output structs are reset per iteration so a failed query cannot
		// leave the previous module's data behind.
		properties = {};
		state = {};
		bandwidth = {};
		(void)getProperties(memoryModules[i], &properties);
		(void)getState(memoryModules[i], &state);
		(void)getBandwidth(memoryModules[i], &bandwidth);
	}

	return ZE_RESULT_SUCCESS;
}
