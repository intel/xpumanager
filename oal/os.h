/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _OS_H
#define _OS_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define UNUSED_VAR(x) (void)(x)
#define UNUSED [[maybe_unused]]
#define GETPROCESSNAME(processId) getProcessName(processId)
#define AMC_CARD_DISCOVERY(amcDeviceList) amcCardDiscovery(amcDeviceList)
#define TIMESTAMP timestamp
#define SETPROGRESS(devIndex, lineNum, totalThreads, progress) setProgress(devIndex, lineNum, totalThreads, progress)
#define GET_XE_DEV_PCI_PROPS(pciPropsList) getXeDevPciProps(pciPropsList)

struct bdfID
{
	uint32_t domain;
	uint32_t bus;
	uint32_t device;
	uint32_t function;
};

struct amcCardInfo
{
	std::string amcDevicePath;
	std::string gpuParentPath;
};

struct xeDevPciInfo
{
	std::string bdf;		 // e.g. "0000:03:00.0"
	std::string pciSlot;	 // e.g. "PCIEX16(G5)"
	int pcieGeneration;		 // e.g. 5
	int maxLinkWidth;		 // e.g. 16
	double maxBandwidthGBps; // e.g. 63.01
};

std::string getProcessName(uint32_t processId);
std::string timestamp();
int amcCardDiscovery(void *amcDeviceList);
int getXeDevPciProps(std::vector<xeDevPciInfo> *pciPropsList);
void setProgress(int devIndex, int lineNum, int totalThreads, uint32_t progress);
inline std::mutex progressPrintMutex;

// Discover a PCI device's hwmon "<prefix>N_input" nodes (OS-specific sysfs
// traversal, kept in the OAL so hal stays portable). Returns a map of
// zero-based sensor index ("<prefix>1_input" -> 0, "<prefix>2_input" -> 1, ...)
// to the absolute "<prefix>N_input" path, taken from the first hwmon node under
// the device that yields a valid entry. The fan RPM fallback passes prefix
// "fan"; hwmon exposes fanN_input directly (there is no fanN_label). The Linux
// implementation lives in oal/lin/hwmon_fan.cpp; the Windows stub
// (oal/win/hwmon_fan.cpp) returns an empty map, so the fan fallback is simply
// absent on Windows.
std::map<uint32_t, std::string> getHwmonInputPaths(const std::string &pciBdf, const std::string &subsystemPrefix);

#ifdef _WIN32
#include "win/oswin.h"
#else
#include "lin/oslin.h"
#endif

#endif
