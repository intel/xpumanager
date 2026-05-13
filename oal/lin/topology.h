/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef TOPOLOGY_H
#define TOPOLOGY_H

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
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
 * @brief Network interface card information for topology matrix
 */
struct NicInfo
{
	std::string name;		 ///< Interface name (e.g. "eth0", "ens3f0")
	std::string bdfAddress;	 ///< PCIe BDF address (e.g. "0000:03:00.0")
	std::string cpuAffinity; ///< Local CPU list string (e.g. "0-31")
};

/**
 * @brief Sysfs path roots used by discoverNics and getNumaNodes — separated for testability
 */
struct SysfsPaths
{
	std::filesystem::path netRoot{"/sys/class/net"};		  ///< Network interface directory
	std::filesystem::path pciDevRoot{"/sys/bus/pci/devices"}; ///< PCI device directory
};

/**
 * @brief Resolves the NUMA node index for each PCI BDF address via sysfs
 *
 * Reads /sys/bus/pci/devices/<BDF>/numa_node for each BDF.
 * Distinguishes three cases:
 * - Key absent:              BDF device directory not found in sysfs
 * - Key present, nullopt:    Device exists but NUMA info unavailable
 *                            (numa_node file absent, value is -1, or UMA system)
 * - Key present, int value:  Resolved NUMA node index
 *
 * @param bdfs  List of PCI BDF strings (e.g. "0000:03:00.0")
 * @param paths Sysfs roots (defaults to live sysfs; override in tests)
 * @return Map from BDF string to optional NUMA node index
 */
std::unordered_map<std::string, std::optional<int>> getNumaNodes(const std::vector<std::string> &bdfs,
																 const SysfsPaths &paths = {});

/// One hop in the PCIe path from the root complex down toward a PCI device.
struct PcieBridgeLink
{
	std::string bdf;   ///< Canonical sysfs path segment for this hop
					   ///< (e.g. "pci0000:00" for the root complex, "0000:00:1c.0" for a bridge)
	bool isHostBridge; ///< true = root complex entry (upstream is CPU), false = PCI-to-PCI bridge

	bool operator==(const PcieBridgeLink &) const = default;
};

/// PCIe bridge chain from root complex (index 0) down toward the device, outermost first.
using PciePath = std::vector<PcieBridgeLink>;

/**
 * @brief Returns the PCIe bridge chain for each BDF by reading sysfs canonical paths
 *
 * Resolves the symlink /sys/bus/pci/devices/<BDF> to its canonical realpath, e.g.:
 *   /sys/devices/pci0000:00/0000:00:1c.0/0000:01:00.0/0000:03:00.0
 *
 * The root complex marker (pciDDDD:BB) is included as element 0 (isHostBridge=true),
 * followed by the intermediate bridge BDFs up to (but not including) the device itself,
 * outermost-first. This enables determineLinkType to classify connections by finding
 * the deepest common ancestor:
 *   - commonLen == 1  → PHB (share only the root complex)
 *   - commonLen == 2  → PXB (share root complex + root port)
 *   - commonLen >= 3  → PIX (share root complex + root port + a PCIe switch)
 *
 * @param bdfs  List of PCI BDF strings (e.g. "0000:03:00.0")
 * @param paths Sysfs roots (defaults to live sysfs; override in tests)
 * @return Map from BDF to PCIe bridge chain; key absent if BDF symlink cannot be resolved
 */
std::unordered_map<std::string, PciePath> getPciePaths(const std::vector<std::string> &bdfs,
													   const SysfsPaths &paths = {});

/**
 * @brief Discovers physical NICs present on the system via sysfs
 *
 * @param paths  sysfs root paths (defaults to live sysfs; override in tests)
 * @return std::nullopt if netRoot is unavailable (sysfs error),
 *         or a (possibly empty) sorted vector of discovered physical NICs
 */
std::optional<std::vector<NicInfo>> discoverNics(const SysfsPaths &paths = {});

/**
 * @brief Exports system topology to XML file using hwloc (Linux only)
 *
 * @param filename Output file path
 * @param gpuDevices List of GPU devices to include in topology
 * @return 0 on success, -1 on failure
 */
int exportTopologyToXml(const std::string &filename, const std::vector<GpuDeviceInfo> &gpuDevices);

#endif // TOPOLOGY_H
