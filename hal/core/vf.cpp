/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "vf.h"
#include <osvf.h>
#include <chrono>
#include <thread>
#include <algorithm>
#include <sstream>

// Interval in milliseconds between engine utilization snapshots
static constexpr int VF_METRICS_INTERVAL_MS = 100;

/**
 * @brief Internal structure for VF engine snapshot data
 */
struct VFEngineSnapshot
{
	zes_engine_group_t engineType;
	uint64_t activeCounterValue;
	uint64_t samplingCounterValue;
};

/**
 * @brief Internal structure for VF snapshot during two-snapshot measurement
 */
struct VFSnapshot
{
	zes_vf_handle_t vfHandle;
	uint32_t vfId;
	uint32_t domain;
	uint8_t bus;
	uint8_t device;
	uint8_t function;
	uint64_t vfDeviceMemSize;
	zes_mem_loc_t memLocation;
	uint64_t memoryUtilized;
	std::vector<VFEngineSnapshot> engineSnapshots;
};

/**
 * @brief Destructor for the Virtual Function (VF) class
 *
 * This destructor performs cleanup operations for the VF management object,
 * releasing allocated memory for enabled VF handle arrays and ensuring
 * proper resource deallocation when the VF object is destroyed.
 */
vf::~vf()
{
	if (vfEnabledHandles) {
		delete[] vfEnabledHandles;
		vfEnabledHandles = nullptr;
	}
}

/**
 * @brief Enumerates all enabled Virtual Functions for a device
 *
 * This function discovers and catalogs all VFs that are enabled on the
 * specified device. Enabled VFs are those that have been configured and
 * are available for activation in the SR-IOV virtualization framework.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
ze_result_t vf::enumEnabledVF(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumEnabledVFExp(device, &vfEnabledCount, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get enabled VF vfEnabledCount. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	if (vfEnabledCount == 0) {
		DBG("No enabled VFs found.\n");
		return ZE_RESULT_SUCCESS;
	}

	vfEnabledHandles = new zes_vf_handle_t[vfEnabledCount];
	result = zesDeviceEnumEnabledVFExp(device, &vfEnabledCount, vfEnabledHandles);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to enumerate enabled VFs. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	return result;
}

/**
 * @brief Gets capabilities and configuration information for a Virtual Function
 *
 * This function retrieves comprehensive capability information for a specific VF,
 * including VF ID, PCI address details (domain, bus, device, function), and
 * device memory size allocation for virtualization management.
 *
 * @param vfHandle Handle to the specific Virtual Function
 * @param outCaps Optional pointer to store capabilities data for caller use
 * @return ze_result_t ZE_RESULT_SUCCESS on successful capability retrieval, error code otherwise
 */
ze_result_t vf::getVFCapabilities(zes_vf_handle_t vfHandle, zes_vf_exp2_capabilities_t *outCaps)
{
	zes_vf_exp2_capabilities_t capabilities = {};
	ze_result_t result = zesVFManagementGetVFCapabilitiesExp2(vfHandle, &capabilities);
	if (result != ZE_RESULT_SUCCESS) {
		DBG("Failed to get VF capabilities. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	if (outCaps) {
		*outCaps = capabilities;
	}

	DBG("Successfully retrieved VF capabilities.\n");
	DBG("  - VF ID: {}\n", capabilities.vfID);
	DBG("  - VF Domain: {}\n", capabilities.address.domain);
	DBG("  - VF Bus: {}\n", capabilities.address.bus);
	DBG("  - VF Device: {}\n", capabilities.address.device);
	DBG("  - VF Function: {}\n", capabilities.address.function);
	DBG("  - VF vfDeviceMemSize: {} bytes\n", (unsigned long long)capabilities.vfDeviceMemSize);

	return result;
}

/**
 * @brief Gets memory utilization statistics for a Virtual Function
 *
 * This function retrieves detailed memory usage information for a specific VF,
 * including memory location (system or device memory) and the amount of
 * memory currently utilized by the VF for performance monitoring.
 *
 * @param vfHandle Handle to the specific Virtual Function
 * @param outMem Optional pointer to store memory utilization data for caller use
 * @return ze_result_t ZE_RESULT_SUCCESS on successful memory utilization retrieval, error code otherwise
 */
ze_result_t vf::getVFMemoryUtilization(zes_vf_handle_t vfHandle, zes_vf_util_mem_exp2_t *outMem)
{
	uint32_t memoryUtilCount = 0;
	zes_vf_util_mem_exp2_t memoryUtil = {};
	ze_result_t result = zesVFManagementGetVFMemoryUtilizationExp2(vfHandle, &memoryUtilCount, &memoryUtil);
	if (result != ZE_RESULT_SUCCESS) {
		DBG("Failed to get VF memory utilization. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Successfully retrieved VF memory utilization.\n");

	if (outMem) {
		*outMem = memoryUtil;
	}
	switch (memoryUtil.vfMemLocation) {
	case ZES_MEM_LOC_SYSTEM:
		DBG("  - VF Memory Location: System\n");
		break;
	case ZES_MEM_LOC_DEVICE:
		DBG("  - VF Memory Location: Device\n");
		break;
	default:
		DBG("  - VF Memory Location: Unknown\n");
		break;
	}

	DBG("  - VF Memory Utilized: {}bytes\n", memoryUtil.vfMemUtilized);

	return result;
}

/**
 * @brief Gets engine utilization statistics for a Virtual Function
 *
 * This function retrieves comprehensive engine usage information for a specific VF,
 * including engine type classification (compute, render, media, copy engines),
 * active counter values, and sampling counter values for detailed performance analysis.
 *
 * @param vfHandle Handle to the specific Virtual Function
 * @param outEngine Optional pointer to store engine utilization data for caller use
 * @return ze_result_t ZE_RESULT_SUCCESS on successful engine utilization retrieval, error code otherwise
 */
ze_result_t vf::getVFEngineUtilization(zes_vf_handle_t vfHandle, zes_vf_util_engine_exp2_t *outEngine)
{
	uint32_t engineUtilCount = 0;
	zes_vf_util_engine_exp2_t engineUtil = {};
	ze_result_t result = zesVFManagementGetVFEngineUtilizationExp2(vfHandle, &engineUtilCount, &engineUtil);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get VF engine utilization. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Successfully retrieved VF engine utilization.\n");

	if (outEngine) {
		*outEngine = engineUtil;
	}
	switch (engineUtil.vfEngineType) {
	case ZES_ENGINE_GROUP_ALL:
		DBG("  - VF Engine Type: All\n");
		break;
	case ZES_ENGINE_GROUP_COMPUTE_ALL:
		DBG("  - VF Engine Type: Compute All\n");
		break;
	case ZES_ENGINE_GROUP_MEDIA_ALL:
		DBG("  - VF Engine Type: Media All\n");
		break;
	case ZES_ENGINE_GROUP_COPY_ALL:
		DBG("  - VF Engine Type: Copy All\n");
		break;
	case ZES_ENGINE_GROUP_COMPUTE_SINGLE:
		DBG("  - VF Engine Type: Compute\n");
		break;
	case ZES_ENGINE_GROUP_RENDER_SINGLE:
		DBG("  - VF Engine Type: Render\n");
		break;
	case ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE:
		DBG("  - VF Engine Type: Media Decode\n");
		break;
	case ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE:
		DBG("  - VF Engine Type: Media Encode\n");
		break;
	case ZES_ENGINE_GROUP_COPY_SINGLE:
		DBG("  - VF Engine Type: Copy\n");
		break;
	case ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE:
		DBG("  - VF Engine Type: Media Enhancement\n");
		break;
	case ZES_ENGINE_GROUP_3D_SINGLE:
		DBG("  - VF Engine Type: 3D\n");
		break;
	case ZES_ENGINE_GROUP_3D_RENDER_COMPUTE_ALL:
		DBG("  - VF Engine Type: 3D Render Compute All\n");
		break;
	case ZES_ENGINE_GROUP_RENDER_ALL:
		DBG("  - VF Engine Type: Render All\n");
		break;
	case ZES_ENGINE_GROUP_3D_ALL:
		DBG("  - VF Engine Type: 3D All\n");
		break;
	case ZES_ENGINE_GROUP_MEDIA_CODEC_SINGLE:
		DBG("  - VF Engine Type: Media Codec\n");
		break;
	default:
		DBG("  - VF Engine Type: Unknown\n");
		break;
	}

	DBG("  - VF Active counter value: {}\n", engineUtil.activeCounterValue);
	DBG("  - VF Sampling counter value: {}\n", engineUtil.samplingCounterValue);

	return result;
}

/**
 * @brief Gets all engine utilization counters for a Virtual Function
 *
 * This function retrieves all engine utilization data for a specific VF,
 * returning a vector of engine snapshots with counters for each engine type.
 *
 * @param vfHandle Handle to the specific Virtual Function
 * @param snapshots Vector to populate with engine snapshot data
 * @return ze_result_t ZE_RESULT_SUCCESS on success, error code otherwise
 */
static ze_result_t getAllVFEngineUtilization(zes_vf_handle_t vfHandle, std::vector<VFEngineSnapshot> &snapshots)
{
	uint32_t engineUtilCount = 0;
	ze_result_t result = zesVFManagementGetVFEngineUtilizationExp2(vfHandle, &engineUtilCount, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get VF engine count. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	if (engineUtilCount == 0) {
		DBG("No engine utilization data available.\n");
		return ZE_RESULT_SUCCESS;
	}

	std::vector<zes_vf_util_engine_exp2_t> engineUtils(engineUtilCount);
	result = zesVFManagementGetVFEngineUtilizationExp2(vfHandle, &engineUtilCount, engineUtils.data());
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get VF engine utilization. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	snapshots.clear();
	snapshots.reserve(engineUtilCount);
	for (const auto &engine : engineUtils) {
		snapshots.push_back({engine.vfEngineType, engine.activeCounterValue, engine.samplingCounterValue});
	}

	return ZE_RESULT_SUCCESS;
}

/*
 * @brief Creates Virtual Functions (VFs) for a device
 *
 * This function allocates and initializes Virtual Functions for the specified
 * device, enabling SR-IOV capabilities and resource management.
 *
 * @param[in] deviceInfo Pointer to the device information structure
 * @return ze_result_t ZE_RESULT_SUCCESS on successful VF creation, error code otherwise
 */
ze_result_t vf::createVFs(DeviceSriovInfo *deviceInfo)
{
	TRACING();

	/* Call OAL Macro for creating VFs */
	if (CREATEVFS(deviceInfo)) {
		ERR("Failed to create VFs.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}
	return ZE_RESULT_SUCCESS;
}

/*
 * @brief Removes Virtual Functions (VFs) for a device
 *
 * This function deallocates and cleans up Virtual Functions for the specified
 * device, disabling SR-IOV capabilities and resource management.
 *
 * @param[in] deviceInfo Pointer to the device information structure
 * @return ze_result_t ZE_RESULT_SUCCESS on successful VF removal, error code otherwise
 */
ze_result_t vf::removeVFs(DeviceSriovInfo *deviceInfo)
{
	TRACING();

	/* Call OAL Macro for removing VFs */
	if (REMOVEVFS(deviceInfo)) {
		ERR("Failed to remove VFs.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}
	return ZE_RESULT_SUCCESS;
}

/*
 * @brief Lists all Virtual Functions (VFs) for a device
 *
 * This function retrieves and populates information about all Virtual Functions
 * associated with the specified device, including their configuration and status.
 *
 * @param[in] deviceInfo Pointer to device information structure
 * @param[out] result Reference to a vector to populate with DeviceSriovInfo structures
 * @return ze_result_t ZE_RESULT_SUCCESS on successful VF listing, error code otherwise
 */
ze_result_t vf::listVFs(DeviceSriovInfo *deviceInfo, std::vector<DeviceSriovInfo> &result)
{
	TRACING();

	/* Call OAL Macro for listing VFs */
	if (LISTVFS(deviceInfo, result)) {
		ERR("Failed to list VFs.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Gets comprehensive statistics for all enabled Virtual Functions
 *
 * This function collects detailed statistics for all enabled VFs on the device,
 * using a two-snapshot method to calculate accurate engine utilization percentages.
 * Engine utilization is aggregated by type (max of each category) and an overall
 * GPU utilization is calculated as the max of all engine types.
 *
 * Note: VFs must be enumerated via init() before calling this function.
 *
 * If VF capabilities API is not supported on the device, BDF information
 * is obtained via LISTVFS OAL call as a fallback.
 *
 * @param[in] deviceInfo Pointer to device info (used for LISTVFS fallback)
 * @param[out] statsList Reference to a vector to populate with VFStatsInfo structures
 * @return ze_result_t ZE_RESULT_SUCCESS on successful statistics retrieval, error code otherwise
 */
ze_result_t vf::getVFStatsList(DeviceSriovInfo *deviceInfo, std::vector<VFStatsInfo> &statsList)
{
	TRACING();

	if (vfEnabledCount == 0 || vfEnabledHandles == nullptr) {
		DBG("No enabled VFs to get stats for. Ensure init() was called.\n");
		return ZE_RESULT_SUCCESS;
	}

	// Get VF list info via LISTVFS OAL call (used as fallback if capabilities API fails)
	std::vector<DeviceSriovInfo> vfListInfo;
	if (deviceInfo != nullptr) {
		if (LISTVFS(deviceInfo, vfListInfo) != 0) {
			DBG("Failed to get VF list info via LISTVFS.\n");
		}
	}

	std::vector<VFSnapshot> snapshots;
	snapshots.reserve(vfEnabledCount);

	for (uint32_t i = 0; i < vfEnabledCount; i++) {
		zes_vf_handle_t vfHandle = vfEnabledHandles[i];
		VFSnapshot snap = {};
		snap.vfHandle = vfHandle;
		snap.vfId = i + 1; // Default VF ID if capabilities not available

		// Try to get VF capabilities via Sysman API
		zes_vf_exp2_capabilities_t caps = {};
		if (getVFCapabilities(vfHandle, &caps) == ZE_RESULT_SUCCESS) {
			snap.vfId = caps.vfID;
			snap.domain = caps.address.domain;
			snap.bus = static_cast<uint8_t>(caps.address.bus);
			snap.device = static_cast<uint8_t>(caps.address.device);
			snap.function = static_cast<uint8_t>(caps.address.function);
			snap.vfDeviceMemSize = caps.vfDeviceMemSize;
		} else {
			// Fallback: get BDF from vfListInfo (index i+1 since index 0 is PF)
			DBG("VF capabilities API not supported, using LISTVFS fallback for VF {}.\n", i);
			uint32_t vfListIndex = i + 1;
			if (vfListIndex < vfListInfo.size()) {
				const auto &vfInfo = vfListInfo[vfListIndex];
				// Parse BDF address string (format: "DDDD:BB:DD.F")
				unsigned int domain = 0, bus = 0, device = 0, function = 0;
				char colon1 = 0, colon2 = 0, dot = 0;
				std::istringstream iss(vfInfo.bdfAddress);
				iss >> std::hex >> domain >> colon1 >> bus >> colon2 >> device >> dot >> function;
				if (iss && colon1 == ':' && colon2 == ':' && dot == '.') {
					snap.domain = domain;
					snap.bus = static_cast<uint8_t>(bus);
					snap.device = static_cast<uint8_t>(device);
					snap.function = static_cast<uint8_t>(function);
				} else {
					DBG("Failed to parse BDF address '{}' for VF {}.\n", vfInfo.bdfAddress.c_str(), i);
				}
			}
		}

		// Try to get VF memory utilization
		zes_vf_util_mem_exp2_t mem = {};
		if (getVFMemoryUtilization(vfHandle, &mem) == ZE_RESULT_SUCCESS) {
			snap.memLocation = mem.vfMemLocation;
			snap.memoryUtilized = mem.vfMemUtilized;
		} else {
			snap.memLocation = ZES_MEM_LOC_FORCE_UINT32;
			snap.memoryUtilized = 0;
		}

		if (getAllVFEngineUtilization(vfHandle, snap.engineSnapshots) != ZE_RESULT_SUCCESS) {
			DBG("Failed to get engine utilization for VF {}.\n", i);
		}

		snapshots.push_back(snap);
	}

	// Wait for metrics interval between snapshots
	std::this_thread::sleep_for(std::chrono::milliseconds(VF_METRICS_INTERVAL_MS));

	statsList.clear();
	statsList.reserve(snapshots.size());

	for (auto &snap : snapshots) {
		VFStatsInfo stats;
		stats.vfId = snap.vfId;
		stats.domain = snap.domain;
		stats.bus = snap.bus;
		stats.device = snap.device;
		stats.function = snap.function;
		stats.vfDeviceMemSize = snap.vfDeviceMemSize;
		stats.memLocation = snap.memLocation;
		stats.memoryUtilized = snap.memoryUtilized;

		stats.gpuUtilization = -1.0;
		stats.computeUtilization = -1.0;
		stats.renderUtilization = -1.0;
		stats.mediaUtilization = -1.0;
		stats.copyUtilization = -1.0;

		// Get second engine utilization snapshot
		std::vector<VFEngineSnapshot> secondSnapshot;
		if (getAllVFEngineUtilization(snap.vfHandle, secondSnapshot) != ZE_RESULT_SUCCESS) {
			DBG("Failed to get second engine snapshot for VF {}.\n", snap.vfId);
			statsList.push_back(stats);
			continue;
		}

		if (secondSnapshot.size() != snap.engineSnapshots.size()) {
			DBG("Engine count mismatch for VF {}.\n", snap.vfId);
			statsList.push_back(stats);
			continue;
		}

		double maxMedia = -1.0, maxCompute = -1.0, maxRender = -1.0, maxCopy = -1.0;

		for (size_t j = 0; j < snap.engineSnapshots.size(); j++) {
			const auto &first = snap.engineSnapshots[j];
			const auto &second = secondSnapshot[j];

			if (first.engineType != second.engineType) {
				DBG("Engine type mismatch at index {} for VF {}.\n", j, snap.vfId);
				continue;
			}

			if (second.samplingCounterValue <= first.samplingCounterValue) {
				DBG("Invalid sampling counter for engine {} on VF {}.\n", first.engineType, snap.vfId);
				continue;
			}

			if (second.activeCounterValue < first.activeCounterValue) {
				DBG("Active counter wrap detected for engine {} on VF {}.\n", first.engineType, snap.vfId);
				continue;
			}

			uint64_t activeDelta = second.activeCounterValue - first.activeCounterValue;
			uint64_t samplingDelta = second.samplingCounterValue - first.samplingCounterValue;
			double utilPercent = (static_cast<double>(activeDelta) / static_cast<double>(samplingDelta)) * 100.0;

			utilPercent = std::min(utilPercent, 100.0);

			switch (first.engineType) {
			case ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE:
			case ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE:
			case ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE:
				maxMedia = std::max(maxMedia, utilPercent);
				break;
			case ZES_ENGINE_GROUP_COMPUTE_SINGLE:
				maxCompute = std::max(maxCompute, utilPercent);
				break;
			case ZES_ENGINE_GROUP_RENDER_SINGLE:
				maxRender = std::max(maxRender, utilPercent);
				break;
			case ZES_ENGINE_GROUP_COPY_SINGLE:
				maxCopy = std::max(maxCopy, utilPercent);
				break;
			default:
				DBG("Unknown engine type {} for VF {}.\n", first.engineType, snap.vfId);
				break;
			}
		}

		stats.mediaUtilization = maxMedia;
		stats.computeUtilization = maxCompute;
		stats.renderUtilization = maxRender;
		stats.copyUtilization = maxCopy;

		double maxOverall = -1.0;
		if (maxMedia >= 0.0)
			maxOverall = std::max(maxOverall, maxMedia);
		if (maxCompute >= 0.0)
			maxOverall = std::max(maxOverall, maxCompute);
		if (maxRender >= 0.0)
			maxOverall = std::max(maxOverall, maxRender);
		if (maxCopy >= 0.0)
			maxOverall = std::max(maxOverall, maxCopy);
		stats.gpuUtilization = maxOverall;

		statsList.push_back(stats);
	}

	DBG("Successfully retrieved stats for {} VFs.\n", statsList.size());
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Checks for VMX support on the system
 *
 * This function determines whether Virtual Machine Extensions (VMX) are
 * supported on the current system, which is a prerequisite for certain
 * virtualization features.
 *
 * @return bool True if VMX is supported, false otherwise
 *
 */
bool vf::vmxSupport()
{
	TRACING();
	bool ret = VMXSUPPORT();
	DBG("VMX is{} supported on this system\n", ret ? "" : " not");
	return ret;
}

/**
 * @brief Checks for IOMMU support on the system
 *
 * This function determines whether Input-Output Memory Management Unit (IOMMU)
 * is supported on the current system, which is essential for secure device
 * virtualization.
 *
 * @return bool True if IOMMU is supported, false otherwise
 */
bool vf::iommuSupport()
{
	TRACING();
	bool ret = IOMMUSUPPORT();
	DBG("IOMMU is{} supported on this system\n", ret ? "" : " not");
	return ret;
}

/**
 * @brief Checks for SR-IOV support on the device
 *
 * This function determines whether Single Root I/O Virtualization (SR-IOV)
 * is supported on the specified device, which is essential for VF functionality.
 *
 * @param[in] deviceInfo Pointer to the device information structure
 * @return bool True if SR-IOV is supported, false otherwise
 */
bool vf::sriovSupport(DeviceSriovInfo *deviceInfo)
{
	TRACING();
	bool ret = SRIOVSUPPORT(deviceInfo);
	DBG("SRIOV is{} supported on this device\n", ret ? "" : " not");
	return ret;
}

/**
 * @brief Initializes the Virtual Function management module for a device
 *
 * This function performs initial setup of VF monitoring capabilities by
 * enumerating enabled Virtual Functions on the specified device for
 * subsequent VF management and monitoring operations.
 *
 * @param device Handle to the device for VF initialization
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t vf::init(zes_device_handle_t device)
{
	ze_result_t result = enumEnabledVF(device);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	DBG("Successfully initialized VF.\n");
	return result;
}

/**
 * @brief Performs comprehensive Virtual Function runtime monitoring operations
 *
 * This function executes a complete VF monitoring cycle for all enabled VFs,
 * including capability retrieval, memory utilization monitoring, and engine
 * utilization tracking for comprehensive SR-IOV performance analysis.
 *
 * @param device Handle to the device (unused in current implementation)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful execution, error code otherwise
 */
ze_result_t vf::zesRun(UNUSED zes_device_handle_t device)
{
	for (uint32_t i = 0; i < vfEnabledCount; i++) {
		zes_vf_handle_t v = vfEnabledHandles[i];
		getVFCapabilities(v);
		getVFMemoryUtilization(v);
		getVFEngineUtilization(v);
	}
	return ZE_RESULT_SUCCESS;
}
