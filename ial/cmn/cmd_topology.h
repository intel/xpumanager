/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CMD_TOPOLOGY_H
#define _CMD_TOPOLOGY_H

#include "cmds.h"
#include "printer.h"
#include <optional>
#include <variant>
#include <os.h>
#include <string>
#include <format>

// Forward declarations
struct portInfo;

/**
 * @brief GPU-tile-specific data within a topology node
 */
struct GpuData
{
	int deviceId;
	int tileId;
	std::vector<portInfo> ports;
};

/**
 * @brief NIC-specific data within a topology node
 */
struct NicData
{
	int nicIndex; ///< Index of this NIC in the discovered NIC list — stable self-identity
};

/**
 * @brief Unified topology node — represents either a GPU tile or a NIC in the matrix.
 *
 * The active alternative in @c data determines the node kind:
 *   - @c GpuData  → GPU tile (possibly a subdevice)
 *   - @c NicData  → Physical network interface card
 */
struct TopoNode
{
	std::string label;		 ///< Display label, e.g. "GPU 0/1" or "NIC0"
	std::string cpuAffinity; ///< Local CPU list string (e.g. "0-31"), used for display
	std::string bdfAddress;	 ///< PCI BDF address (e.g. "0000:03:00.0"), used for NUMA lookup

	std::optional<int> numaNode{std::nullopt}; ///< sysfs NUMA node index (nullopt = unknown)
	std::optional<PciePath> pciePath{}; ///< PCIe bridge chain from sysfs canonical path walk (nullopt = not resolved)
	std::optional<int64_t> maxBandwidthBps{
		std::nullopt}; ///< PCIe max bandwidth in bytes/sec from ZES (nullopt = unknown or NIC)

	std::variant<GpuData, NicData> data; ///< Kind-specific payload; holds_alternative<GpuData> ↔ GPU
};

/**
 * @brief Device topology information structure
 *
 * Contains NUMA and PCIe topology details for a specific GPU device.
 */
struct TopologyInfo
{
	uint32_t deviceId;		  ///< Device identifier
	std::string localCpuList; ///< Comma-separated list of local CPU core IDs
	std::string localCpus;	  ///< CPU affinity mask in hexadecimal format
	int pcieSwitchCount;	  ///< Number of PCIe switches in the path
	std::string pcieSwitch;	  ///< PCIe switch device path or "N/A"
};

/**
 * @brief Serializes TopologyInfo to JSON format
 *
 * @param[in] info The topology information structure to serialize
 * @return Ordered JSON object containing topology data
 */
inline nlohmann::ordered_json toJson(const TopologyInfo &info)
{
	return {{"device_id", info.deviceId},
			{"local_cpu_list", info.localCpuList},
			{"local_cpus", info.localCpus},
			{"pcie_switch_count", info.pcieSwitchCount},
			{"pcie_switch", info.pcieSwitch}};
}

/**
 * @brief Formats TopologyInfo as human-readable text
 *
 * @param[in] info The topology information to format
 * @return Formatted multi-line string with labeled fields
 */
inline std::string toText(const TopologyInfo &info)
{
	return std::format("Device ID: {}\nLocal CPU List: {}\nLocal CPUs: {}\n"
					   "PCIe Switch Count: {}\nPCIe Switch: {}\n",
					   info.deviceId, info.localCpuList, info.localCpus, info.pcieSwitchCount, info.pcieSwitch);
}

/**
 * @brief Text printer specialized for topology command output
 *
 * Provides formatted text output for topology data structures using
 * both legacy JSON-based printing and modern type-safe printing.
 */
class TopologyTextPrinter : public TextPrinter
{
public:
	/**
	 * @brief Default constructor
	 */
	TopologyTextPrinter();

	/**
	 * @brief Prints topology data from JSON object (legacy support)
	 *
	 * @param[in] jsonObj Pointer to JSON object containing topology data
	 */
	void print(nlohmann::ordered_json *jsonObj) override;

	/**
	 * @brief Prints TopologyInfo as formatted text
	 *
	 * @param[in] info The topology information to print
	 */
	void print(const TopologyInfo &info);
};

enum class topologyCmdType
{
	TOPOLOGY_HELP,
	TOPOLOGY_JSON,
	TOPOLOGY_DEVICE,
	TOPOLOGY_FILE,
	TOPOLOGY_MATRIX,
	TOTAL_TOPOLOGY,
};

class cmdTopology : public cmds // NOLINT(readability-identifier-naming) // camelBack for consistency
{
private:
	ze_result_t buildTopologyMatrix(arg_struct *args, nlohmann::ordered_json *jsonObj) const;
	std::string xmlFilename; // Store filename for XML export

public:
	// NOLINTNEXTLINE(readability-identifier-naming) // camelBack for consistency
	cmdTopology() { name = "topology"; };
	void help(HELP helpType = FULL_HELP) final;

	// Public for unit testing
	static std::string determineLinkType(const TopoNode &node1, const TopoNode &node2);

	/**
	 * @brief Generates an XML topology file with system topology information
	 *
	 * Creates a comprehensive topology file containing GPU devices, CPU nodes,
	 * interconnect details, and NUMA relationships. Prints result directly.
	 *
	 * @param[in] filename Path to the output XML file
	 * @param[in] useJson If true, output as JSON; otherwise as text
	 * @return ZE_RESULT_SUCCESS on success, error code on failure
	 */
	[[nodiscard]] ze_result_t generateFile(const std::string &filename, bool useJson);

	/**
	 * @brief Generates and displays the system topology matrix
	 *
	 * Creates a matrix showing connectivity and relationships between all GPU devices,
	 * CPU nodes, and interconnects using symbolic notation. Prints result directly.
	 *
	 * @param[in] useJson If true, output as JSON; otherwise as text
	 * @return ZE_RESULT_SUCCESS on success, error code on failure
	 */
	[[nodiscard]] ze_result_t showMatrix(bool useJson);

	/**
	 * @brief Executes the topology command with parsed command line arguments
	 *
	 * @param[in] args Pointer to argument structure containing argc, argv, and system manager
	 * @return ZE_RESULT_SUCCESS on success, error code on failure
	 */
	int run(arg_struct *args) final;

private:
	arg_struct *currentArgs = nullptr; ///< Cached pointer to command arguments

	/**
	 * @brief Displays topology information for a specified device
	 *
	 * Retrieves CPU affinity, NUMA information, and PCIe topology details
	 * for the specified GPU device.
	 *
	 * @param[in] d Pointer to device information structure
	 * @param[out] info Output parameter to receive topology information
	 * @return ZE_RESULT_SUCCESS on success, error code on failure
	 */
	[[nodiscard]] static ze_result_t showTopology(devInfo *d, TopologyInfo *info);
};

/**
 * @brief Structure for topology command options
 *
 * Stores command line option information including the option specification,
 * whether it has been enabled by the user, and any associated value.
 */
struct TopologyCmdStruct
{
	bool enabled{false}; ///< Whether this option was provided by the user
	std::string val{};	 ///< Value associated with the option (if required)
};

#endif
