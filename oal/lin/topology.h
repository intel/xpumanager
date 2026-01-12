/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _TOPOLOGY_H
#define _TOPOLOGY_H

#include <string>
#include <vector>

/**
 * @brief GPU device information for topology export
 */
struct GpuDeviceInfo
{
	int deviceIndex;
	std::string bdfAddress;
	std::string cpuAffinity;
};

/**
 * @brief Exports system topology to XML file using hwloc (Linux only)
 *
 * @param filename Output file path
 * @param gpuDevices List of GPU devices to include in topology
 * @return 0 on success, -1 on failure
 */
int exportTopologyToXml(const std::string &filename, const std::vector<GpuDeviceInfo> &gpuDevices);

#endif // _TOPOLOGY_H
