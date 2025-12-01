/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _OSVF_H
#define _OSVF_H

#include <cinttypes>
#include <string>
#include <zes_api.h>

#define ONE_MB_IN_BYTES (1024 * 1024)

enum devFuncType
{
	DEVICE_FUNCTION_TYPE_UNKNOWN,
	DEVICE_FUNCTION_TYPE_VIRTUAL,
	DEVICE_FUNCTION_TYPE_PHYSICAL,
	DEVICE_FUNCTION_TYPE_ALL,
};

struct AttrFromConfigFile
{
	bool driversAutoprobe;
	bool schedIfIdle;
	uint64_t vfLmem;
	uint64_t vfLmemEcc;
	uint32_t vfContexts;
	uint32_t vfDoorbells;
	uint64_t vfGgtt;
	uint64_t vfExec;
	uint64_t vfPreempt;
	uint64_t pfExec;
	uint64_t pfPreempt;
};

struct DeviceSriovInfo
{
	uint32_t vGpuNumber;
	uint64_t vGpuMemorySize;
	zes_device_ecc_state_t eccState;
	std::string drmPath;
	std::string bdfAddress;
	uint64_t lmemSizeFree;
	uint64_t ggttSizeFree;
	uint32_t doorbellFree;
	uint32_t contextFree;
	devFuncType functionType;
};

// PCI Configuration Space offsets
#ifndef PCI_CAPABILITY_LIST
#define PCI_CAPABILITY_LIST 0x34
#endif

#ifndef PCI_CAP_ID_SRIOV
#define PCI_CAP_ID_SRIOV 0x10
#endif

/**
 * @brief PCI device information structure with SRIOV support
 */
struct PciDeviceInfo
{
	// Basic PCI device information
	uint32_t domain;
	uint8_t bus;
	uint8_t dev;
	uint8_t func;
	uint16_t vendorId;
	uint16_t deviceId;
	uint16_t subvendorId;
	uint16_t subdeviceId;
	uint32_t deviceClass;
	uint8_t revision;

	std::string driver;
	bool valid;

	// SRIOV-specific information
	bool isSriovCapable;
	uint16_t totalVFs;		// Total VFs supported
	uint16_t initialVFs;	// Initial VFs
	uint16_t numVFs;		// Current number of VFs enabled
	uint32_t vfStride;		// VF stride
	uint16_t vfDeviceId;	// VF device ID
	uint32_t firstVFOffset; // First VF offset
	uint32_t vfBarSize[6];	// VF BAR sizes
};

/**
 * @brief Comprehensive VF statistics data
 *
 * Contains all statistics information for a single Virtual Function,
 * including identification, memory usage, and engine utilization percentages.
 * Utilization values are in percentage (0.0 - 100.0), or -1.0 if not available.
 */
struct VFStatsInfo
{
	uint32_t vfId = 0;
	uint32_t domain = 0;
	uint8_t bus = 0;
	uint8_t device = 0;
	uint8_t function = 0;

	uint64_t vfDeviceMemSize = 0;					// Memory quota allocated to VF in bytes
	zes_mem_loc_t memLocation = ZES_MEM_LOC_SYSTEM; // System or device memory
	uint64_t memoryUtilized = 0;					// Current memory usage in bytes

	// Engine utilization percentages, value of -1.0 indicates data not available
	double gpuUtilization = -1.0;	  // Overall GPU utilization (max of all engines)
	double computeUtilization = -1.0; // Compute engine utilization
	double renderUtilization = -1.0;  // Render engine utilization
	double mediaUtilization = -1.0;	  // Media engine utilization (max of decode/encode/enhancement)
	double copyUtilization = -1.0;	  // Copy engine utilization
};

#endif
