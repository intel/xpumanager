/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "topology.h"
#include "bdf.h"
#include <hwloc.h>
#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>
#include <utility>

/**
 * @brief Resolves the NUMA node index for each PCI BDF address via sysfs
 *
 * Reads /sys/bus/pci/devices/<BDF>/numa_node for each BDF.
 * This is simpler and faster than the hwloc cpuset walk.
 *
 * @param bdfs  List of PCI BDF strings (e.g. "0000:03:00.0")
 * @param paths Sysfs roots (defaults to live sysfs; override in tests)
 * @return Map from BDF string to optional NUMA node index:
 *   - Key absent:           BDF device directory not found in sysfs
 *   - Key present, nullopt: Device exists but NUMA info unavailable
 *                           (numa_node file absent, value is -1 on multi-NUMA,
 *                           or sysfs nodeRoot unreadable)
 *   - Key present, int:     Resolved NUMA node index; on UMA systems
 *                           (possible=="0", sysfs readable) numa_node = -1 is
 *                           canonicalised to 0
 */
std::unordered_map<std::string, std::optional<int>> getNumaNodes(const std::vector<std::string> &bdfs,
																 const SysfsPaths &paths)
{
	namespace fs = std::filesystem;

	// Read /sys/devices/system/node/possible to determine if this is a UMA system.
	// Checking for node1 is unreliable: it may be absent on kernels compiled without
	// NUMA support (sysfs absent entirely) or before a CXL node hot-plugs in.
	// "possible" is the authoritative source: "0" means only one node exists (UMA).
	// If the file is absent or unreadable, leave isUma=false so -1 → nullopt (safe).
	const bool isUma = [&] {
		std::ifstream f{paths.nodeRoot / "possible"};
		std::string line;
		if (!f || !std::getline(f, line)) {
			return false;
		}
		if (const auto pos = line.find_last_not_of(" \t\r\n"); pos != std::string::npos) {
			line.resize(pos + 1);
		}
		return line == "0";
	}();

	std::unordered_map<std::string, std::optional<int>> result;
	for (const auto &bdf : bdfs) {
		const fs::path devDir = paths.pciDevRoot / bdf;
		std::error_code ec;
		if (!fs::exists(devDir, ec)) {
			continue; // BDF not present in sysfs — leave key absent
		}
		const fs::path numaPath = devDir / "numa_node";
		if (std::ifstream ifs{numaPath}; ifs) {
			int idx{};
			if (!(ifs >> idx)) {
				result[bdf] = std::nullopt;
			} else if (idx >= 0) {
				result[bdf] = idx;
			} else if (idx == -1 && isUma) {
				result[bdf] = 0; // UMA: canonicalise to node 0
			} else {
				result[bdf] = std::nullopt;
			}
		} else {
			result[bdf] = std::nullopt; // device exists but NUMA info unavailable
		}
	}
	return result;
}

/**
 * @brief Returns the PCIe bridge chain for each BDF by reading sysfs canonical paths
 *
 * See topology.h for full algorithm description.
 */
std::unordered_map<std::string, PciePath> getPciePaths(const std::vector<std::string> &bdfs, const SysfsPaths &paths)
{
	namespace fs = std::filesystem;
	std::unordered_map<std::string, PciePath> result;

	for (const auto &bdf : bdfs) {
		std::error_code ec;
		const fs::path real = fs::canonical(paths.pciDevRoot / bdf, ec);
		if (ec) {
			continue;
		}

		// Walk canonical path segments. The root complex marker "pciDDDD:BB" is
		// captured as element 0; intermediate bridges follow; the device BDF itself
		// is excluded (popped).
		std::string rootComplexSeg;
		std::vector<std::string> bridgeSegs;
		bool foundRoot = false;
		for (const auto &part : real) {
			const std::string seg = part.string();
			if (!foundRoot) {
				if (seg.starts_with("pci")) {
					foundRoot = true;
					rootComplexSeg = seg; // e.g. "pci0000:00"
				}
				continue;
			}
			bridgeSegs.push_back(seg);
		}
		if (!bridgeSegs.empty()) {
			bridgeSegs.pop_back(); // drop the device BDF itself
		}

		if (rootComplexSeg.empty()) {
			continue; // malformed canonical path
		}

		PciePath path;
		path.push_back(PcieBridgeLink{.bdf = rootComplexSeg, .isHostBridge = true});
		for (const auto &seg : bridgeSegs) {
			path.push_back(PcieBridgeLink{.bdf = seg, .isHostBridge = false});
		}

		if (!path.empty()) {
			result[bdf] = std::move(path);
		}
	}

	return result;
}

/**
 * @brief Discovers physical NICs present on the system via sysfs
 *
 * Enumerates /sys/class/net, filters to PCI-backed physical interfaces,
 * reads each NIC's BDF address and local CPU affinity from sysfs.
 * Loopback ("lo") and virtual (non-PCI) interfaces are skipped.
 *
 * @return std::nullopt if /sys/class/net is unavailable (sysfs error),
 *         or a (possibly empty) sorted vector of NicInfo
 */
std::optional<std::vector<NicInfo>> discoverNics(const SysfsPaths &paths)
{
	namespace fs = std::filesystem;

	std::error_code fsErr;
	if (!fs::exists(paths.netRoot, fsErr)) {
		return std::nullopt; // sysfs unavailable
	}

	std::error_code openErr;
	auto dirIt = fs::directory_iterator(paths.netRoot, openErr);
	if (openErr) {
		return std::nullopt; // failed to open directory (e.g. permission denied)
	}

	const fs::directory_iterator end;

	// Advance the iterator; returns false and sets result to nullopt on error.
	const auto advance = [&](std::optional<std::vector<NicInfo>> &out) -> bool {
		std::error_code incErr;
		dirIt.increment(incErr);
		if (incErr) {
			out = std::nullopt;
			return false;
		}
		return true;
	};

	std::optional<std::vector<NicInfo>> result{std::in_place};
	while (dirIt != end) {
		const auto &entry = *dirIt;
		const std::string name = entry.path().filename().string();

		if (name == "lo") {
			if (!advance(result)) {
				return result;
			}
			continue; // skip loopback
		}

		// Only include physical PCI-backed interfaces
		const fs::path deviceLink = entry.path() / "device";
		std::error_code existsErr;
		if (!fs::exists(deviceLink, existsErr)) {
			if (!advance(result)) {
				return result;
			}
			continue;
		}

		// Resolve BDF from the symlink target filename
		std::error_code symlinkErr;
		const auto target = fs::read_symlink(deviceLink, symlinkErr);
		if (symlinkErr) {
			if (!advance(result)) {
				return result;
			}
			continue;
		}
		const std::string bdf = target.filename().string();

		// Skip entries whose device symlink doesn't resolve to a PCI BDF
		if (!isValidBdf(bdf)) {
			if (!advance(result)) {
				return result;
			}
			continue;
		}

		// Read local CPU list from sysfs PCI entry
		std::string cpuAffinity;
		const fs::path cpuListPath = paths.pciDevRoot / bdf / "local_cpulist";
		if (std::ifstream ifs{cpuListPath}; ifs) {
			std::getline(ifs, cpuAffinity);
		}

		result->push_back(NicInfo{.name = name, .bdfAddress = bdf, .cpuAffinity = cpuAffinity});

		if (!advance(result)) {
			return result;
		}
	}

	std::ranges::sort(*result, {}, &NicInfo::name);
	return result;
}

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
			if (gpu != nullptr) {
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
