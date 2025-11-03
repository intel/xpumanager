/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _OSVF_H
#define _OSVF_H

#include "os.h"
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

#endif
