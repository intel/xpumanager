/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "memory.h"

/**
 * @brief Destructor for the memory class
 *
 * This destructor performs cleanup operations for the memory management
 * object, releasing allocated memory for memory module handles and ensuring
 * proper resource deallocation when the memory object is destroyed.
 */
memory::~memory()
{
	if (memoryModules) {
		delete[] memoryModules;
		memoryModules = nullptr;
	}
}

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
	ze_result_t result = zesDeviceEnumMemoryModules(device, &memoryModulesCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || memoryModulesCount == 0) {
		ERR("Failed to enumerate Memory modules. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	memoryModules = new zes_mem_handle_t[memoryModulesCount];
	result = zesDeviceEnumMemoryModules(device, &memoryModulesCount, memoryModules);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get Memory modules. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Found %d memory modules\n", memoryModulesCount);
	return result;
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
ze_result_t memory::getProperties(zes_mem_handle_t memhandle, zes_mem_properties_t *properties)
{
	ze_result_t result = zesMemoryGetProperties(memhandle, properties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get Memory properties. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Memory properties retrieved successfully.\n");

	DBG("Memory type: ");
	switch (properties->type) {
	case ZES_MEM_TYPE_HBM:
		DBG("HBM\n");
		break;
	case ZES_MEM_TYPE_DDR:
		DBG("DDR\n");
		break;
	case ZES_MEM_TYPE_DDR3:
		DBG("DDR3\n");
		break;
	case ZES_MEM_TYPE_DDR4:
		DBG("DDR4\n");
		break;
	case ZES_MEM_TYPE_DDR5:
		DBG("DDR5\n");
		break;
	case ZES_MEM_TYPE_LPDDR:
		DBG("LPDDR\n");
		break;
	case ZES_MEM_TYPE_LPDDR3:
		DBG("LPDDR3\n");
		break;
	case ZES_MEM_TYPE_LPDDR4:
		DBG("LPDDR4\n");
		break;
	case ZES_MEM_TYPE_LPDDR5:
		DBG("LPDDR5\n");
		break;
	case ZES_MEM_TYPE_SRAM:
		DBG("SRAM\n");
		break;
	case ZES_MEM_TYPE_L1:
		DBG("L1\n");
		break;
	case ZES_MEM_TYPE_L3:
		DBG("L3\n");
		break;
	case ZES_MEM_TYPE_GRF:
		DBG("GRF\n");
		break;
	case ZES_MEM_TYPE_SLM:
		DBG("SLM\n");
		break;
	case ZES_MEM_TYPE_GDDR4:
		DBG("GDDR4\n");
		break;
	case ZES_MEM_TYPE_GDDR5:
		DBG("GDDR5\n");
		break;
	case ZES_MEM_TYPE_GDDR5X:
		DBG("GDDR5X\n");
		break;
	case ZES_MEM_TYPE_GDDR6:
		DBG("GDDR6\n");
		break;
	case ZES_MEM_TYPE_GDDR6X:
		DBG("GDDR6X\n");
		break;
	case ZES_MEM_TYPE_GDDR7:
		DBG("GDDR7\n");
		break;
	default:
		DBG("Other\n");
		break;
	}

	DBG("Subdevice ID: %d\n", properties->subdeviceId);
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
	DBG("Physical size: %" PRIu64 " bytes\n", properties->physicalSize);
	DBG("Bus width: %d bits\n", properties->busWidth);
	DBG("Number of channels: %d\n", properties->numChannels);

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
	ze_result_t result = zesMemoryGetState(memhandle, state);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get Memory state. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Memory state retrieved successfully.\n");
	DBG("Free memory: %" PRIu64 " bytes\n", state->free);
	DBG("Size: %" PRIu64 " bytes\n", state->size);
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
		ERR("Failed to get Memory bandwidth. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Memory bandwidth retrieved successfully.\n");
	DBG("Read counter: %" PRIu64 " bytes\n", bandwidth->readCounter);
	DBG("Write counter: %" PRIu64 " bytes\n", bandwidth->writeCounter);
	DBG("Max bandwidth: %" PRIu64 " bytes/sec\n", bandwidth->maxBandwidth);
	DBG("Timestamp: %" PRIu64 " ns\n", bandwidth->timestamp);

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
	zes_mem_state_t state;

	if (size == nullptr) {
		ERR("Size pointer is null.\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}
	*size = 0;

	for (uint32_t i = 0; i < memoryModulesCount; i++) {
		result = getState(memoryModules[i], &state);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory state for module %d. 0x%X (%s)\n", i, result, l0_error_to_string(result));
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
	zes_mem_state_t state;

	if (health == nullptr) {
		ERR("Health pointer is null.\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}
	*health = ZES_MEM_HEALTH_UNKNOWN;

	for (uint32_t i = 0; i < memoryModulesCount; i++) {
		result = getState(memoryModules[i], &state);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory state for module %d. 0x%X (%s)\n", i, result, l0_error_to_string(result));
			return result;
		}
		if (state.health == ZES_MEM_HEALTH_CRITICAL || state.health == ZES_MEM_HEALTH_REPLACE) {
			*health = state.health;
			DBG("Memory health is critical or replace for module %d.\n", i);
			return result;
		} else if (state.health == ZES_MEM_HEALTH_DEGRADED) {
			*health = ZES_MEM_HEALTH_DEGRADED;
			DBG("Memory health is degraded for module %d.\n", i);
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
			DBG("Memory health is OK for module %d.\n", i);
		} else {
			*health = ZES_MEM_HEALTH_UNKNOWN;
			DBG("Memory health is unknown for module %d.\n", i);
			return result;
		}
	}
	return result;
}

/**
 * @brief Retrieves memory channel configuration information
 *
 * This function queries the number of memory channels available across
 * memory modules. Memory channels represent parallel data paths that
 * affect memory bandwidth and performance characteristics.
 *
 * @param channels Pointer to variable that will receive number of memory channels
 * @return ze_result_t ZE_RESULT_SUCCESS on successful channel retrieval, error code otherwise
 */
ze_result_t memory::getMemoryChannels(uint32_t *channels)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_mem_properties_t properties;

	if (channels == nullptr) {
		ERR("Channels pointer is null.\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}
	*channels = 0;

	for (uint32_t i = 0; i < memoryModulesCount; i++) {
		result = getProperties(memoryModules[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory properties for module %d. 0x%X (%s)\n", i, result, l0_error_to_string(result));
			return result;
		}
		DBG("Memory properties for module %d: numChannels=%d\n", i, properties.numChannels);
		*channels = properties.numChannels;
	}
	return result;
}

/**
 * @brief Retrieves memory bus width configuration
 *
 * This function queries the memory bus width in bits across memory modules.
 * Bus width determines the amount of data that can be transferred per clock
 * cycle and directly affects memory bandwidth performance.
 *
 * @param busWidth Pointer to variable that will receive memory bus width in bits
 * @return ze_result_t ZE_RESULT_SUCCESS on successful bus width retrieval, error code otherwise
 */
ze_result_t memory::getMemoryBusWidth(uint32_t *busWidth)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_mem_properties_t properties;

	if (busWidth == nullptr) {
		ERR("Bus width pointer is null.\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}
	*busWidth = 0;

	for (uint32_t i = 0; i < memoryModulesCount; i++) {
		result = getProperties(memoryModules[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory properties for module %d. 0x%X (%s)\n", i, result, l0_error_to_string(result));
			return result;
		}
		DBG("Memory properties for module %d: busWidth=%d\n", i, properties.busWidth);
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

	for (uint32_t i = 0; i < memoryModulesCount; i++) {
		zes_mem_properties_t properties = {};
		zes_mem_state_t state = {};

		result = getProperties(memoryModules[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory properties for module %d. 0x%X (%s)\n", i, result, l0_error_to_string(result));
			return result;
		}
		result = getState(memoryModules[i], &state);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory state for module %d. 0x%X (%s)\n", i, result, l0_error_to_string(result));
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

	for (uint32_t i = 0; i < memoryModulesCount; i++) {

		result = getBandwidth(memoryModules[i], &bandwidth);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory bandwidth for module %d. 0x%X (%s)\n", i, result, l0_error_to_string(result));
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

	for (uint32_t i = 0; i < memoryModulesCount; i++) {
		zes_mem_properties_t properties = {};
		result = getProperties(memoryModules[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory properties for module %d. 0x%X (%s)\n", i, result, l0_error_to_string(result));
			continue;
		}

		if (properties.location != ZES_MEM_LOC_DEVICE) {
			continue;
		}

		zes_mem_bandwidth_t bandwidth = {};
		result = getBandwidth(memoryModules[i], &bandwidth);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory bandwidth for module %d. 0x%X (%s)\n", i, result, l0_error_to_string(result));
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

	for (uint32_t i = 0; i < memoryModulesCount; i++) {
		zes_mem_properties_t properties = {};
		result = getProperties(memoryModules[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory properties for module %d. 0x%X (%s)\n", i, result, l0_error_to_string(result));
			continue;
		}

		if (properties.location != ZES_MEM_LOC_DEVICE) {
			continue;
		}

		zes_mem_state_t state = {};
		result = getState(memoryModules[i], &state);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get Memory state for module %d. 0x%X (%s)\n", i, result, l0_error_to_string(result));
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
 * @brief Initializes the memory management subsystem
 *
 * This function performs initialization of the memory management system by
 * enumerating all available memory modules on the specified device. It serves
 * as the entry point for memory subsystem setup and configuration.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t memory::init(zes_device_handle_t device)
{
	TRACING();
	return enumMemoryModules(device);
}

/**
 * @brief Executes comprehensive memory system diagnostics and data collection
 *
 * This function performs a complete memory system scan by retrieving properties,
 * state, and bandwidth information for all memory modules. It serves as a
 * diagnostic routine for memory subsystem health and performance assessment.
 *
 * @param device Handle to the Level Zero Sysman device (unused in current implementation)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful diagnostic completion
 */
ze_result_t memory::zesRun(UNUSED zes_device_handle_t device)
{
	zes_mem_state_t state = {};
	zes_mem_properties_t properties = {};
	zes_mem_bandwidth_t bandwidth = {};

	for (uint32_t i = 0; i < memoryModulesCount; i++) {
		getProperties(memoryModules[i], &properties);
		getState(memoryModules[i], &state);
		getBandwidth(memoryModules[i], &bandwidth);
	}

	return ZE_RESULT_SUCCESS;
}
