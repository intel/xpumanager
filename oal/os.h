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
uint32_t getCurrentProcessId();
std::string timestamp();
int amcCardDiscovery(void *amcDeviceList);
int getXeDevPciProps(std::vector<xeDevPciInfo> *pciPropsList);
void setProgress(int devIndex, int lineNum, int totalThreads, uint32_t progress);
inline std::mutex progressPrintMutex;

// Discover the hwmon nodes exposed by a PCI device and return, for the given
// sensor-subsystem prefix, a map of {hwmon label -> absolute "<prefix>N_input"
// path}. On Linux this walks /sys/bus/pci/devices/<pciBdf>/hwmon/hwmon* and, for
// every strict "<subsystemPrefix><N>_label" file, records the label against its
// sibling "<subsystemPrefix><N>_input" path (first match wins). For temperature
// the caller passes subsystemPrefix "temp" and looks up labels such as "pkg" /
// "vram"; the prefix is parameterized so the discovery is reusable for other
// hwmon subsystems. On platforms without sysfs (Windows) this returns an empty
// map.
std::map<std::string, std::string> getHwmonLabelPaths(const std::string &pciBdf, const std::string &subsystemPrefix);

#ifdef _WIN32
#include "win/oswin.h"
#else
#include "lin/oslin.h"
#endif

#endif
