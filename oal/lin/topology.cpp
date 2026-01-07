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

#include "topology.h"
#include <hwloc.h>
#include <format>

/**
 * @brief Exports system topology to XML file using hwloc
 *
 * This function creates a comprehensive system topology XML file containing
 * CPU, memory, and device information. It uses hwloc to discover system
 * topology and adds Intel GPU devices as misc objects with metadata.
 *
 * @param filename Path to output XML file
 * @param gpuDevices Vector of GPU device information to include
 * @return 0 on success, -1 on failure
 */
int exportTopologyToXml(const std::string &filename, const std::vector<GpuDeviceInfo> &gpuDevices)
{
	// Initialize hwloc topology
	hwloc_topology_t topology{};
	if (hwloc_topology_init(&topology) != 0) {
		return -1;
	}

	// Enable PCI device discovery (bridges, GPUs, NICs, storage, etc.)
	// Must be called before hwloc_topology_load()
	if (hwloc_topology_set_io_types_filter(topology, HWLOC_TYPE_FILTER_KEEP_ALL) != 0) {
		hwloc_topology_destroy(topology);
		return -1;
	}

	// Also enable all types to ensure we get memory modules and other misc objects
	if (hwloc_topology_set_all_types_filter(topology, HWLOC_TYPE_FILTER_KEEP_ALL) != 0) {
		hwloc_topology_destroy(topology);
		return -1;
	}

	// Load the topology
	if (hwloc_topology_load(topology) != 0) {
		hwloc_topology_destroy(topology);
		return -1;
	}

	// Add GPU devices to topology
	if (!gpuDevices.empty()) {
		hwloc_obj_t root = hwloc_get_root_obj(topology);
		for (const auto &device : gpuDevices) {
			const std::string deviceName = std::format("Intel GPU Device {}", device.deviceIndex);

			// Add GPU as misc object with metadata
			hwloc_obj_t gpu = hwloc_topology_insert_misc_object(topology, root, deviceName.c_str());
			if (gpu) {
				hwloc_obj_add_info(gpu, "Type", "GPU");
				hwloc_obj_add_info(gpu, "PCIBusID", device.bdfAddress.c_str());
				hwloc_obj_add_info(gpu, "CPUAffinity", device.cpuAffinity.c_str());
				hwloc_obj_add_info(gpu, "Vendor", "Intel Corporation");
			}
		}
	}

	// Export topology to XML using hwloc's native export function
	const int result = hwloc_topology_export_xml(topology, filename.c_str(), 0);

	// Clean up hwloc topology
	hwloc_topology_destroy(topology);

	return result;
}
