/*
 * Copyright © 2026 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
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
