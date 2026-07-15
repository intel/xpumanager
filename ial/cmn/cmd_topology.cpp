/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_topology.h"
#include "debug.h"
#include <CLI/CLI.hpp>
#include "printer.h"
#include "table_builder.h"
#include "ze_api.h"
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <format>
#include <algorithm>
#include <memory>
#include <map>
#include <ranges>
#include <optional>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <variant>

namespace {
constexpr int MATRIX_TILE_COL_WIDTH = 9;
constexpr int MATRIX_CONNECTION_COL_WIDTH = 7;
constexpr int MATRIX_AFFINITY_COL_WIDTH = 15;

/**
 * @brief CPU information structure for parsed /proc/cpuinfo data
 */
struct CpuInfo
{
	std::string model;
	int family = 0;
	int modelNumber = 0;
	int stepping = 0;

	auto operator<=>(const CpuInfo &) const = default;
};

/// Per-device fabric port data used only by the "n" (MDF) P2P capability check.
struct FabricPortData
{
	std::vector<std::vector<zes_fabric_port_id_t>> localPortIds;
	std::vector<std::vector<portInfo>> allPorts;
};

/// Single source of truth for P2P capability key → human-readable description.
/// Used by both printP2PLegend and printP2PCapabilityHelp.
/// "w" is an accepted alias for "r"; both map to Read (Level Zero unified capability).
constexpr auto P2P_CAP_DESCRIPTIONS = std::to_array<std::pair<std::string_view, std::string_view>>({
	{"r", "P2P read/write access (\"w\" accepted as alias; Level Zero reports as a unified capability)"},
	{"n", "MDF fabric connectivity"},
	{"a", "P2P atomic operations"},
	{"p", "PCIe P2P access"},
});
} // namespace

/**
 * @defgroup topology_commands Topology Commands
 * @brief System topology query and analysis commands for GPU devices
 *
 * Provides commands to query device topology information including:
 * - CPU affinity and NUMA node relationships
 * - PCIe switch topology
 * - GPU interconnect connectivity (MDF)
 * - Topology matrix visualization
 * - Topology export to XML files
 */

/**
 * @defgroup topology_matrix Topology Matrix Generation
 * @ingroup topology_commands
 * @brief Functions for generating and analyzing GPU interconnect topology matrices
 *
 * Matrix generation examines fabric ports, PCIe topology, and CPU affinity
 * to determine connectivity types between all GPU tiles in the system.
 * Supports multiple link types: MDF, PCIe (NODE/SYS).
 */

/**
 * @defgroup topology_printers Topology Output Formatting
 * @ingroup topology_commands
 * @brief Printer classes for formatting topology data in various output formats
 *
 * Handles conversion of topology data structures to human-readable text
 * or JSON format for programmatic consumption.
 */

/**
 * @brief Constructor for TopologyTextPrinter
 * @ingroup topology_printers
 */
TopologyTextPrinter::TopologyTextPrinter() = default;

/**
 * @brief Prints the P2P matrix legend beneath the capability matrix.
 *
 * Looks up the human-readable description for the requested capability from
 * P2P_CAP_DESCRIPTIONS.  Falls back to the raw capability key when no matching
 * entry is found.
 *
 * @param[in] jsonObj JSON object containing a @c "p2p_capability" string field.
 *
 * @post Legend is printed to stdout via PRINT.
 */
static void printP2PLegend(nlohmann::ordered_json *jsonObj)
{
	const auto cap = jsonObj->value("p2p_capability", std::string{});
	const auto it = std::ranges::find_if(P2P_CAP_DESCRIPTIONS, [&cap](const auto &p) { return p.first == cap; });
	const std::string desc = (it != P2P_CAP_DESCRIPTIONS.end()) ? std::string{it->second} : cap;
	PRINT("\nCapability: {}\nLegend:\n  X  : Self\n  OK : Supported\n  NS : Not supported\n  ?  : Query failed (driver "
		  "or device error)\n",
		  desc.c_str());
}

/**
 * @brief Renders a topology or P2P matrix JSON object as a bordered text table.
 *
 * Dispatches on the @c "p2p" boolean sentinel in @p jsonObj:
 *  - Topology matrix: appends a "CPU Affinity" column sized to the longest value.
 *  - P2P matrix: omits the affinity column; appends the legend via printP2PLegend().
 *
 * @param[in] jsonObj JSON object containing @c "headers" (array) and @c "matrix"
 *                    (array of objects with @c "tile", @c "connections", and
 *                    optionally @c "cpu_affinity").  Must not be nullptr.
 *
 * @pre  @p jsonObj contains both @c "matrix" and @c "headers" keys.
 * @post Formatted table (and optional P2P legend) is printed to stdout via PRINT.
 */
static void printMatrixBlock(nlohmann::ordered_json *jsonObj)
{
	const auto &headers = (*jsonObj)["headers"];
	const auto &matrix = (*jsonObj)["matrix"];
	const bool isP2P = jsonObj->value("p2p", false);

	TableBuilder table;
	table.addColumn("", MATRIX_TILE_COL_WIDTH);
	for (const auto &header : headers) {
		const auto &name = header.get<std::string>();
		const int colWidth = std::max(MATRIX_CONNECTION_COL_WIDTH, static_cast<int>(name.size()));
		table.addColumn(name, colWidth);
	}

	if (!isP2P) {
		int affinityWidth = MATRIX_AFFINITY_COL_WIDTH;
		for (const auto &row : matrix) {
			const auto affinity = row["cpu_affinity"].get<std::string>();
			affinityWidth = std::max(affinityWidth, static_cast<int>(affinity.size()));
		}
		table.addColumn("CPU Affinity", affinityWidth);
	}

	for (const auto &row : matrix) {
		std::vector<std::string> cells;
		cells.push_back(row["tile"].get<std::string>());
		for (const auto &conn : row["connections"]) {
			cells.push_back(conn.get<std::string>());
		}
		if (!isP2P) {
			cells.push_back(row["cpu_affinity"].get<std::string>());
		}
		table.addRowFromContainer(cells);
	}

	PRINT("{}", table.toString().c_str());

	if (isP2P) {
		printP2PLegend(jsonObj);
	}
}

/**
 * @brief Custom text printer for topology command output (legacy JSON support)
 * @ingroup topology_printers
 *
 * Formats JSON topology data into human-readable text with proper labels and alignment.
 * Supports multiple output formats: error messages, simple text messages, topology matrix,
 * and device-specific information.
 *
 * Supported Formats:
 * 1. Error: {"error": "message"} → "Error: message"
 * 2. Message: {"message": "text"} → "text"
 * 3. Matrix: {"headers": [...], "matrix": [...]} → Bordered table via TableBuilder with:
 *    - Dynamic columns: one per tile header plus "CPU Affinity"
 *    - Data rows with connection types (S/MDF/NODE/SYS) and CPU affinity per tile
 * 4. Device: {"device_id": N, "local_cpu_list": "...", ...} → 2-column TableBuilder
 *    table with "Property" and "Value" columns, auto-sized with max cell width of 120
 *
 * @param[in] jsonObj Pointer to JSON object containing topology data.
 *                    Must not be nullptr.
 *                    Expected fields depend on format:
 *                    - Error format: "error" (string)
 *                    - Message format: "message" (string)
 *                    - Matrix format: "headers" (array), "matrix" (array of objects)
 *                      Matrix objects contain: "tile", "connections" (array), "cpu_affinity"
 *                    - Device format: "device_id", "local_cpu_list", "local_cpus",
 *                      "pcie_switch_count", "pcie_switch"
 *
 * @pre jsonObj must be a valid pointer to initialized nlohmann::ordered_json
 * @pre For matrix format: headers and matrix arrays must be same length
 * @pre For matrix format: each matrix entry connections array must match headers length
 *
 * @post Output is printed to stdout via PRINT macro
 * @post No modification to jsonObj (const behavior despite non-const signature)
 *
 * @note Outputs directly to stdout using PRINT macro
 * @note Format precedence: error > message > matrix > device fields
 * @note Both matrix and device formats use TableBuilder for bordered table output
 * @note Empty fields in device format are silently skipped
 * @note Inherited from TextPrinter base class
 *
 * @see showMatrix() for matrix data generation
 * @see TopologyInfo for device topology structure
 */
void TopologyTextPrinter::print(nlohmann::ordered_json *jsonObj)
{
	TRACING();

	if (jsonObj->contains("error")) {
		PRINT("{}", std::format("Error: {}\n", jsonObj->value("error", "")).c_str());
		return;
	}

	if (const auto msg = jsonObj->value("message", ""); !msg.empty()) {
		PRINT("{}", std::format("{}\n", msg).c_str());
		return;
	}

	if (jsonObj->contains("matrix") && jsonObj->contains("headers")) {
		printMatrixBlock(jsonObj);
		return;
	}

	TableBuilder table;
	table.addColumn("Property", 20);
	table.addColumn("Value", 40);
	table.enableAutoSizing();
	table.setMaxCellWidth(120);

	if (const auto deviceId = jsonObj->value("device_id", -1); deviceId != -1) {
		table.addRow("Device ID", std::to_string(deviceId));
	}
	if (const auto cpuList = jsonObj->value("local_cpu_list", std::string{}); !cpuList.empty()) {
		table.addRow("Local CPU List", cpuList);
	}
	if (const auto localCpus = jsonObj->value("local_cpus", std::string{}); !localCpus.empty()) {
		table.addRow("Local CPUs", localCpus);
	}
	if (jsonObj->contains("pcie_switch_count")) {
		table.addRow("PCIe Switch Count", std::to_string(jsonObj->value("pcie_switch_count", 0)));
	}
	if (const auto pcieSwitch = jsonObj->value("pcie_switch", std::string{}); !pcieSwitch.empty()) {
		table.addRow("PCIe Switch", pcieSwitch);
	}

	PRINT("{}", table.toString().c_str());
}

/**
 * @brief Prints TopologyInfo as a formatted table
 * @ingroup topology_printers
 *
 * @param[in] info The topology information to print
 */
void TopologyTextPrinter::print(const TopologyInfo &info)
{
	TableBuilder table;
	table.addColumn("Property", 20);
	table.addColumn("Value", 40);
	table.enableAutoSizing();
	table.setMaxCellWidth(120);

	table.addRow("Device ID", std::to_string(info.deviceId));
	table.addRow("Local CPU List", info.localCpuList);
	table.addRow("Local CPUs", info.localCpus);
	table.addRow("PCIe Switch Count", std::to_string(info.pcieSwitchCount));
	table.addRow("PCIe Switch", info.pcieSwitch);

	PRINT("{}", table.toString().c_str());
}

static std::unordered_map<topologyCmdType, TopologyCmdStruct> topologyCmds = {
	{topologyCmdType::TOPOLOGY_HELP, {}},	{topologyCmdType::TOPOLOGY_JSON, {}},
	{topologyCmdType::TOPOLOGY_DEVICE, {}}, {topologyCmdType::TOPOLOGY_FILE, {}},
	{topologyCmdType::TOPOLOGY_MATRIX, {}}, {topologyCmdType::TOPOLOGY_P2P, {}},
};

/**
 * @brief Displays help information for the topology command
 * @ingroup topology_commands
 *
 * This function prints comprehensive usage information for the topology command,
 * including options for device queries, matrix display, and file generation.
 * It also explains the topology connection symbols used in the output (XL, SYS, NODE, MDF).
 *
 * @param[in] helpType The type of help to display:
 *                     - FULL_HELP: Complete help with all details and examples
 *                     - SHORT_HELP: Brief usage summary
 *
 * @note Outputs help text directly to stdout
 * @note Includes usage examples and connection symbol legend
 */
void cmdTopology::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.emplace_back(TITLE, "Get the system topology");
	helpList.emplace_back(BLANK);
	helpList.emplace_back(TITLE, "Usage: %s topology [Options]", progName.c_str());
	helpList.emplace_back(HEADING, "%s topology --device [deviceId]", progName.c_str());
	helpList.emplace_back(HEADING, "%s topology --device [pciBdfAddress]", progName.c_str());
	helpList.emplace_back(HEADING, "%s topology --device [deviceId] -j", progName.c_str());
	helpList.emplace_back(HEADING, "%s topology -f [filename]", progName.c_str());
	helpList.emplace_back(HEADING, "%s topology -m", progName.c_str());
	helpList.emplace_back(BLANK);
	helpList.emplace_back(TITLE, "Options:");
	helpList.emplace_back(HEADING, "-h,--help                   Print this help message and exit");
	helpList.emplace_back(HEADING, "-j,--json                   Print result in JSON format");
	helpList.emplace_back(BLANK);
	helpList.emplace_back(HEADING, "--device,--id               The device ID or PCI BDF address to query");
	helpList.emplace_back(HEADING,
						  "-f,--file                   Generate the system topology with the GPU info to a XML file");
	helpList.emplace_back(HEADING, "-m,--matrix                 Print the CPU/GPU/NIC topology matrix");
	helpList.emplace_back(SUB_HEADING, "S: Self");
	helpList.emplace_back(SUB_HEADING, "MDF:  Connected with Multi-Die Fabric Interface");
	helpList.emplace_back(SUB_HEADING, "PIX:  Connected via PCIe switch");
	helpList.emplace_back(SUB_HEADING, "PXB:  Connected via multiple PCIe bridges");
	helpList.emplace_back(SUB_HEADING, "PHB:  Connected via PCIe host bridge");
	helpList.emplace_back(SUB_HEADING, "NODE: Connected within a NUMA node");
	helpList.emplace_back(SUB_HEADING, "SYS:  Worst-case connectivity (cross-NUMA or topology unknown)");
	helpList.emplace_back(HEADING, "--p2p <capability>          Print the P2P capability matrix between GPU devices");

	helpList.emplace_back(SUB_HEADING, "Capability values:");
	{
		TableBuilder capTable;
		capTable.addColumn("", 1).addColumn("", 1).enableAutoSizing();
		for (const auto &[key, desc] : P2P_CAP_DESCRIPTIONS) {
			capTable.addRow("  " + std::string{key}, desc);
		}
		capTable.lockWidths();
		for (const auto &[key, desc] : P2P_CAP_DESCRIPTIONS) {
			helpList.emplace_back(SUB_HEADING, capTable.rowLine({"  " + std::string{key}, std::string{desc}}).c_str());
		}
	}

	helpList.emplace_back(SUB_HEADING, "Matrix symbols:");
	{
		static constexpr auto matrixLegend = std::to_array<std::pair<std::string_view, std::string_view>>({
			{"X", "Self"},
			{"OK", "Capability supported"},
			{"NS", "Not supported"},
			{"?", "Query failed (driver or device error)"},
		});
		TableBuilder symTable;
		symTable.addColumn("", 1).addColumn("", 1).enableAutoSizing();
		for (const auto &[sym, meaning] : matrixLegend) {
			symTable.addRow("  " + std::string{sym}, meaning);
		}
		symTable.lockWidths();
		for (const auto &[sym, meaning] : matrixLegend) {
			helpList.emplace_back(SUB_HEADING,
								  symTable.rowLine({"  " + std::string{sym}, std::string{meaning}}).c_str());
		}
	}

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Displays topology information for a specified device
 * @ingroup topology_commands
 *
 * This function retrieves and displays topology information for a specific GPU device,
 * including CPU affinity information such as the local CPU list and CPU mask.
 * It provides details about which CPUs are local to the device for NUMA optimization,
 * PCIe switch count, and switch device path information.
 *
 * @param[in]  d    Pointer to device information structure containing device details.
 *                  Must not be nullptr. The d->dev field must also be valid.
 *                  The structure provides access to device index and methods to query
 *                  CPU affinity and PCIe topology.
 * @param[out] info Output parameter to receive topology information. Must not be nullptr.
 *                  On success, will be populated with:
 *                  - deviceId: Device index from d->index
 *                  - localCpuList: Comma-separated list of local CPU cores
 *                  - localCpus: CPU affinity mask in hexadecimal format
 *                  - pcieSwitchCount: Number of PCIe switches in the path to root
 *                  - pcieSwitch: Device path of PCIe switch or "N/A" if not applicable
 *
 * @retval ZE_RESULT_SUCCESS               Topology information successfully retrieved
 * @retval ZE_RESULT_ERROR_INVALID_NULL_POINTER  d, d->dev, or info is nullptr
 *
 * @note Uses device methods: getCPUList(), getLocalCPUs(), getSwitchCount()
 */
ze_result_t cmdTopology::showTopology(devInfo *d, TopologyInfo *info)
{
	TRACING();

	if (d == nullptr || d->dev == nullptr) {
		ERR("Invalid device pointer\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}

	if (info == nullptr) {
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}

	std::string switchDevicePath = "N/A";
	const auto cpuList = d->dev->getCPUList();
	const auto localCPUs = d->dev->getLocalCPUs();
	const auto switchCount = d->dev->getSwitchCount(&switchDevicePath);

	*info = TopologyInfo{.deviceId = d->index,
						 .localCpuList = cpuList,
						 .localCpus = localCPUs,
						 .pcieSwitchCount = switchCount,
						 .pcieSwitch = switchDevicePath};

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Generates a topology file containing system topology information
 * @ingroup topology_commands
 *
 * This function creates an XML file containing comprehensive system topology
 * information including GPU devices, CPU nodes, interconnect details, and
 * NUMA relationships. The generated file can be used for system analysis
 * and topology visualization tools. Reads topology directly from sysfs.
 *
 * @param[in] filename Path to the output XML file. Must not be empty.
 *                     Can be absolute or relative path. The file will be created
 *                     or overwritten if it exists.
 * @param[in] useJson  Output format selector:
 *                     - true: Print confirmation as JSON: {"filename": "...", "message": "..."}
 *                     - false: Print confirmation as plain text
 *
 * @retval ZE_RESULT_SUCCESS                 File generation initiated successfully
 * @retval ZE_RESULT_ERROR_INVALID_ARGUMENT  filename is empty string
 * @retval ZE_RESULT_ERROR_UNINITIALIZED     currentArgs is nullptr (internal error)
 *
 * @note Output is written to an xml file via the `-f {filename}` flag
 * @note Requires currentArgs to be set
 *
 * @return ze_result_t ZE_RESULT_SUCCESS on success, error code on failure
 */
ze_result_t cmdTopology::generateFile(const std::string &filename, bool useJson)
{
	TRACING();

	if (filename.empty()) {
		ERR("No filename specified\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (currentArgs == nullptr) {
		ERR("Internal error: args not available\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	// Get GPU devices for topology export
	std::vector<devInfo> deviceList;
	const auto findResult = currentArgs->sm.findDevice("", &deviceList);

	std::vector<GpuDeviceInfo> gpuDevices;
	if (findResult == ZE_RESULT_SUCCESS && !deviceList.empty()) {
		for (const auto &device : deviceList) {
			GpuDeviceInfo gpuInfo;
			gpuInfo.deviceIndex = device.index;
			gpuInfo.bdfAddress = device.dev->getBDFStr();
			gpuInfo.cpuAffinity = device.dev->getCPUList();
			gpuDevices.push_back(gpuInfo);
		}
	}

	// Export topology to XML using OAL function (hwloc / linux only)
	const int exportResult = EXPORT_TOPOLOGY_XML(filename, gpuDevices);
	if (exportResult != 0) {
		ERR("Failed to export topology to file '{}'\n", filename.c_str());
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	const std::string message = std::format("Topology exported to file: {}", filename);

	if (useJson) {
		auto jsonObj = std::make_unique<nlohmann::ordered_json>();
		(*jsonObj)["message"] = message;
		auto printer = std::make_unique<JsonPrinter>();
		printer->print(jsonObj.get());
	} else {
		PRINT("{}\n", message.c_str());
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Builds the topology connectivity matrix for all devices
 * @ingroup topology_matrix
 *
 * Constructs a comprehensive topology matrix showing connectivity between all GPU tiles
 * and NICs. Uses two data sources:
 *   1. sysfs @c numa_node file — for NODE / SYS classification
 *   2. sysfs canonical path walk — for PIX / PXB / PHB classification (when available)
 *
 * @param[in]  args    Pointer to argument structure containing system manager.
 * @param[out] jsonObj Populated with:
 *                     - @c "topo_list": flat array of per-pair topology entries
 *                     - @c "headers":   tile/NIC label array
 *                     - @c "matrix":    2-D connection-type array for text display
 *                     On error: @c "error" string.
 *
 * @retval ZE_RESULT_SUCCESS           Matrix built successfully
 * @retval ZE_RESULT_ERROR_DEVICE_LOST No devices found
 *
 * @note Connection symbols: S, MDF, PIX, PXB, PHB, NODE, SYS
 * @see determineLinkType() for classification algorithm
 */
ze_result_t cmdTopology::buildTopologyMatrix(arg_struct *args, nlohmann::ordered_json *jsonObj) const
{
	TRACING();
	// Get all devices from system manager
	std::vector<devInfo> deviceList;
	const auto result = args->sm.findDevice("", &deviceList);
	if (result != ZE_RESULT_SUCCESS || deviceList.empty()) {
		*jsonObj = {{"error", "No devices found or error getting device list"}};
		return result != ZE_RESULT_SUCCESS ? result : ZE_RESULT_ERROR_DEVICE_LOST;
	}

	// Collect all GPU tiles as topology nodes
	std::vector<TopoNode> allNodes;

	for (auto &device : deviceList) {
		const auto cpuAffinity = device.dev->getCPUList();
		const auto bdf = device.dev->getBDFStr();
		std::map<uint32_t, std::vector<portInfo>> portsByTile;

		// Read PCIe max bandwidth once per device; shared by all tiles on this device.
		std::optional<int64_t> gpuMaxBwBps;
		if (pci *const pciObj = device.dev->getPCI()) {
			zes_pci_properties_t pciProps{};
			if (pciObj->getProperties(device.zesDeviceHdl, &pciProps) == ZE_RESULT_SUCCESS &&
				pciProps.maxSpeed.maxBandwidth > 0) {
				gpuMaxBwBps = static_cast<int64_t>(pciProps.maxSpeed.maxBandwidth);
			}
		}

		fabric *f = device.dev->getFabric();
		if (f != nullptr) {
			std::vector<portInfo> portInfoList;
			if (f->getFabricPorts(device.zesDeviceHdl, portInfoList) == ZE_RESULT_SUCCESS) {
				for (const auto &pi : portInfoList) {
					const uint32_t tileId = (pi.portProps.onSubdevice != 0u) ? pi.portProps.subdeviceId : 0U;
					portsByTile[tileId].push_back(pi);
				}
			}
		}

		if (portsByTile.empty()) {
			allNodes.push_back(
				TopoNode{.label = std::format("GPU {}/0", device.index),
						 .cpuAffinity = cpuAffinity,
						 .bdfAddress = bdf,
						 .maxBandwidthBps = gpuMaxBwBps,
						 .data = GpuData{.deviceId = static_cast<int>(device.index), .tileId = 0, .ports = {}}});
		} else {
			for (const auto &[tileId, ports] : portsByTile) {
				allNodes.push_back(TopoNode{.label = std::format("GPU {}/{}", device.index, tileId),
											.cpuAffinity = cpuAffinity,
											.bdfAddress = bdf,
											.maxBandwidthBps = gpuMaxBwBps,
											.data = GpuData{.deviceId = static_cast<int>(device.index),
															.tileId = static_cast<int>(tileId),
															.ports = ports}});
			}
		}
	}

	// Append NIC nodes
	if (const auto nicsOpt = GET_SYSTEM_NICS()) {
		for (auto nicIdx = size_t{0}; nicIdx < nicsOpt->size(); ++nicIdx) {
			const auto &nic = (*nicsOpt)[nicIdx];
			allNodes.push_back(TopoNode{.label = std::format("NIC{}", nicIdx),
										.cpuAffinity = nic.cpuAffinity,
										.bdfAddress = nic.bdfAddress,
										.data = NicData{.nicIndex = static_cast<int>(nicIdx)}});
		}
	}

	// Resolve NUMA and PCIe bridge paths — de-duplicate BDFs first to avoid
	// redundant sysfs reads for multi-tile GPUs that share a BDF.
	{
		std::vector<std::string> bdfs;
		bdfs.reserve(allNodes.size());
		for (const auto &node : allNodes) {
			bdfs.push_back(node.bdfAddress);
		}
		std::sort(bdfs.begin(), bdfs.end());
		bdfs.erase(std::unique(bdfs.begin(), bdfs.end()), bdfs.end());

		const auto numaMap = GET_NUMA_NODES(bdfs);
		for (auto &node : allNodes) {
			if (const auto it = numaMap.find(node.bdfAddress); it != numaMap.end()) {
				node.numaNode = it->second;
			}
		}

		const auto pcieMap = GET_PCIE_PATHS(bdfs);
		for (auto &node : allNodes) {
			if (const auto it = pcieMap.find(node.bdfAddress); it != pcieMap.end()) {
				node.pciePath = it->second;
			}
		}
	}

	// Build header list
	std::vector<std::string> headers;
	headers.reserve(allNodes.size());
	for (const auto &node : allNodes) {
		headers.push_back(node.label);
	}

	// Build topology list (for JSON) and matrix (for text display)
	const auto nodeCount = allNodes.size();
	std::vector<nlohmann::ordered_json> topoList;
	std::vector<nlohmann::ordered_json> matrixData;

	for (const auto row : std::views::iota(size_t{0}, nodeCount)) {
		nlohmann::ordered_json rowData;
		rowData["tile"] = allNodes[row].label;
		rowData["cpu_affinity"] = allNodes[row].cpuAffinity;

		std::vector<std::string> connections;
		for (const auto col : std::views::iota(size_t{0}, nodeCount)) {
			const std::string linkType = determineLinkType(allNodes[row], allNodes[col]);
			connections.push_back(linkType);

			nlohmann::ordered_json topoEntry;
			const auto *rowGpu = std::get_if<GpuData>(&allNodes[row].data);
			const auto *colGpu = std::get_if<GpuData>(&allNodes[col].data);
			topoEntry["link_type"] = linkType;
			topoEntry["local_node_type"] = rowGpu != nullptr ? "GPU" : "NIC";
			topoEntry["local_cpu_affinity"] = allNodes[row].cpuAffinity;
			topoEntry["local_device_id"] =
				rowGpu != nullptr ? nlohmann::ordered_json(rowGpu->deviceId) : nlohmann::ordered_json(nullptr);
			topoEntry["local_numa_index"] = allNodes[row].numaNode.has_value()
												? nlohmann::ordered_json(*allNodes[row].numaNode)
												: nlohmann::ordered_json(nullptr);
			topoEntry["local_on_subdevice"] = (rowGpu != nullptr);
			topoEntry["local_subdevice_id"] =
				rowGpu != nullptr ? nlohmann::ordered_json(rowGpu->tileId) : nlohmann::ordered_json(nullptr);
			topoEntry["max_bit_rate"] = allNodes[row].maxBandwidthBps.has_value()
											? nlohmann::ordered_json(*allNodes[row].maxBandwidthBps)
											: nlohmann::ordered_json(nullptr);
			topoEntry["remote_node_type"] = colGpu != nullptr ? "GPU" : "NIC";
			topoEntry["remote_device_id"] =
				colGpu != nullptr ? nlohmann::ordered_json(colGpu->deviceId) : nlohmann::ordered_json(nullptr);
			topoEntry["remote_subdevice_id"] =
				colGpu != nullptr ? nlohmann::ordered_json(colGpu->tileId) : nlohmann::ordered_json(nullptr);
			topoList.push_back(topoEntry);
		}
		rowData["connections"] = connections;
		matrixData.push_back(rowData);
	}

	(*jsonObj)["topo_list"] = topoList;
	(*jsonObj)["headers"] = headers;
	(*jsonObj)["matrix"] = matrixData;

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Determines the connection type between two topology nodes
 * @ingroup topology_matrix
 *
 * Uses a priority-ordered decision chain:
 *
 *  1. **S**    — same stable identity (GPU: matching deviceId/tileId; NIC: matching nicIndex)
 *  2. **MDF**  — both GPU tiles on the same physical device
 *  3. **PIX**  — PCIe paths share ≥ 3 common bridge ancestors (same PCIe switch)
 *  4. **PXB**  — PCIe paths share exactly 2 common ancestors (same root port)
 *  5. **PHB**  — PCIe paths share exactly 1 common ancestor (same host bridge)
 *  6. **NODE** — PCIe paths share no common ancestor but both nodes are on the same NUMA node
 *  7. **SYS**  — PCIe paths share no common ancestor, different or unknown NUMA
 *  8. **NODE** — at least one PCIe path absent; both nodes resolve to the same NUMA index
 *  9. **SYS**  — everything else (different NUMA nodes, or NUMA unknown)
 *
 * PCIe path data comes from the sysfs canonical path walk (unavailable on Windows),
 * in which case the fallback NODE / SYS classification is used.
 *
 * @param[in] node1 First topology node
 * @param[in] node2 Second topology node
 * @return One of: "S", "MDF", "PIX", "PXB", "PHB", "NODE", "SYS"
 */
std::string cmdTopology::determineLinkType(const TopoNode &node1, const TopoNode &node2)
{
	TRACING();

	// Self — compare stable node identity rather than display label
	if (const auto *g1 = std::get_if<GpuData>(&node1.data)) {
		if (const auto *g2 = std::get_if<GpuData>(&node2.data)) {
			if (g1->deviceId == g2->deviceId && g1->tileId == g2->tileId) {
				return "S";
			}
		}
	} else if (const auto *n1 = std::get_if<NicData>(&node1.data)) {
		if (const auto *n2 = std::get_if<NicData>(&node2.data)) {
			if (n1->nicIndex == n2->nicIndex) {
				return "S";
			}
		}
	}

	// MDF: two GPU tiles on the same physical device
	if (const auto *g1 = std::get_if<GpuData>(&node1.data)) {
		if (const auto *g2 = std::get_if<GpuData>(&node2.data)) {
			if (g1->deviceId == g2->deviceId && g1->tileId != g2->tileId) {
				return "MDF";
			}
		}
	}

	// PCIe path comparison: find the deepest common bridge ancestor.
	// Path is root-first (index 0 = root complex, index 1 = root port, ...).
	//   commonLen == 1  → PHB (share only the root complex)
	//   commonLen == 2  → PXB (share root complex + root port)
	//   commonLen >= 3  → PIX (share a PCIe switch)
	if (node1.pciePath.has_value() && node2.pciePath.has_value()) {
		const auto &p1 = *node1.pciePath;
		const auto &p2 = *node2.pciePath;
		size_t commonLen = 0;
		for (size_t i = 0; i < std::min(p1.size(), p2.size()); ++i) {
			if (p1[i].bdf == p2[i].bdf) {
				commonLen = i + 1;
			} else {
				break;
			}
		}
		if (commonLen >= 3) {
			return "PIX";
		}
		if (commonLen == 2) {
			return "PXB";
		}
		if (commonLen == 1) {
			return "PHB";
		}
		// commonLen == 0: both paths exist but share no common ancestor; use NUMA as tiebreaker
		if (node1.numaNode.has_value() && node1.numaNode == node2.numaNode) {
			return "NODE";
		}
		return "SYS";
	}

	// Fall back to NUMA comparison only when PCIe path data is absent for at
	// least one node. Two nullopt nodes must not be treated as co-located.
	if (!node1.pciePath.has_value() || !node2.pciePath.has_value()) {
		if (node1.numaNode.has_value() && node1.numaNode == node2.numaNode) {
			return "NODE";
		}
	}

	return "SYS";
}

/**
 * @brief Displays the topology connectivity matrix for all GPU devices and NICs
 * @ingroup topology_matrix
 *
 * @param[in] useJson If true, output JSON format; if false, output text table
 *
 * @retval ZE_RESULT_SUCCESS           Matrix displayed successfully
 * @retval ZE_RESULT_ERROR_DEVICE_LOST No devices found in system
 * @retval ZE_RESULT_ERROR_UNINITIALIZED currentArgs not set
 *
 * @note Connection symbols: S, MDF, PIX, PXB, PHB, NODE, SYS
 * @see buildTopologyMatrix(), determineLinkType()
 */
ze_result_t cmdTopology::showMatrix(bool useJson)
{
	TRACING();

	if (currentArgs == nullptr) {
		ERR("Internal error: currentArgs not initialized\n");
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	nlohmann::ordered_json jsonObj;
	const auto result = buildTopologyMatrix(currentArgs, &jsonObj);

	if (result != ZE_RESULT_SUCCESS) {
		if (useJson) {
			auto jsonPrinter = std::make_unique<JsonPrinter>();
			jsonPrinter->print(&jsonObj);
		} else {
			auto textPrinter = std::make_unique<TopologyTextPrinter>();
			textPrinter->print(&jsonObj);
		}
		return result;
	}

	// Output the matrix
	if (useJson) {
		auto jsonPrinter = std::make_unique<JsonPrinter>();
		jsonPrinter->print(&jsonObj);
	} else {
		auto textPrinter = std::make_unique<TopologyTextPrinter>();
		textPrinter->print(&jsonObj);
	}

	return ZE_RESULT_SUCCESS;
}

// ─── P2P helpers (file-scope, used only by buildP2PMatrix) ───────────────────

/**
 * @brief Collects fabric port data for every device in @p deviceList.
 *
 * Queries the fabric HAL for each device to retrieve port identifiers and full
 * port state.  Devices without a fabric HAL, or for which the HAL query fails,
 * contribute empty vectors at their respective indices.
 *
 * @param[in] deviceList Devices to query; indices are preserved in the returned data.
 * @return FabricPortData where @c localPortIds[i] and @c allPorts[i] are populated
 *         for reachable devices and empty for unreachable ones.
 */
static FabricPortData collectFabricPorts(const std::vector<devInfo> &deviceList)
{
	FabricPortData data{
		.localPortIds = std::vector<std::vector<zes_fabric_port_id_t>>(deviceList.size()),
		.allPorts = std::vector<std::vector<portInfo>>(deviceList.size()),
	};
	for (size_t i = 0; i < deviceList.size(); ++i) {
		fabric *const f = deviceList[i].dev->getFabric();
		if (f == nullptr) {
			continue;
		}
		std::vector<portInfo> ports;
		if (f->getFabricPorts(deviceList[i].zesDeviceHdl, ports) != ZE_RESULT_SUCCESS) {
			continue;
		}
		std::ranges::transform(ports, std::back_inserter(data.localPortIds[i]),
							   [](const portInfo &p) { return p.portProps.portId; });
		data.allPorts[i] = std::move(ports);
	}
	return data;
}

/**
 * @brief Returns whether any active port in @p srcPorts links to the destination device.
 *
 * "Active" means @c ZES_FABRIC_PORT_STATUS_HEALTHY or @c ZES_FABRIC_PORT_STATUS_DEGRADED.
 * Matching uses @c (fabricId, attachId) rather than @c portNumber, since @c portNumber
 * differs on each end of a link.
 *
 * @param[in] srcPorts   All fabric ports belonging to the source device.
 * @param[in] dstPortIds Local port IDs of the destination device.
 * @return @c true if at least one active source port has its remote end within
 *         @p dstPortIds; @c false otherwise (including when either vector is empty).
 */
static bool isFabricConnected(const std::vector<portInfo> &srcPorts,
							  const std::vector<zes_fabric_port_id_t> &dstPortIds)
{
	return std::ranges::any_of(srcPorts, [&dstPortIds](const portInfo &port) {
		const bool active = port.portState.status == ZES_FABRIC_PORT_STATUS_HEALTHY ||
							port.portState.status == ZES_FABRIC_PORT_STATUS_DEGRADED;
		return active && std::ranges::any_of(dstPortIds, [&port](const zes_fabric_port_id_t &pid) {
				   return port.portState.remotePortId.fabricId == pid.fabricId &&
						  port.portState.remotePortId.attachId == pid.attachId;
			   });
	});
}

/**
 * @brief Queries whether a specific P2P capability is supported between two devices.
 *
 * @param[in] capability  The P2P capability to check.
 * @param[in] src         Level Zero handle for the source device.
 * @param[in] dst         Level Zero handle for the destination device.
 * @param[in] srcPorts    Pre-collected fabric port data for @p src (used only for Fabric).
 * @param[in] dstPortIds  Pre-collected fabric port IDs for @p dst (used only for Fabric).
 * @return @c true  if the capability is confirmed supported,
 *         @c false if the capability is confirmed not supported,
 *         @c nullopt if the Level Zero API call failed (displayed as "?" in the matrix).
 */
static std::optional<bool> queryP2PSupported(P2PCapability capability, ze_device_handle_t src, ze_device_handle_t dst,
											 const std::vector<portInfo> &srcPorts,
											 const std::vector<zes_fabric_port_id_t> &dstPortIds)
{
	switch (capability) {
	case P2PCapability::Read: {
		ze_bool_t canAccess = 0;
		if (zeDeviceCanAccessPeer(src, dst, &canAccess) != ZE_RESULT_SUCCESS) {
			return std::nullopt;
		}
		return canAccess != 0;
	}
	case P2PCapability::Fabric:
		return isFabricConnected(srcPorts, dstPortIds);
	case P2PCapability::Atomics:
	case P2PCapability::Pcie: {
		ze_device_p2p_properties_t props{};
		props.stype = ZE_STRUCTURE_TYPE_DEVICE_P2P_PROPERTIES;
		if (zeDeviceGetP2PProperties(src, dst, &props) != ZE_RESULT_SUCCESS) {
			return std::nullopt;
		}
		const auto flag = (capability == P2PCapability::Atomics) ? ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS
																 : ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS;
		return (props.flags & static_cast<ze_device_p2p_property_flag_t>(flag)) != 0;
	}
	}
	return std::nullopt;
}

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Returns the canonical single-letter CLI key for a @c P2PCapability.
 *
 * This is the inverse of @c parseP2PCapability for display and JSON output.
 * @c Read emits @c "r"; @c "w" is a parse-time alias and is never returned here.
 *
 * @param[in] cap  The capability to convert.
 * @return A non-owning string view of the one-character key, or an empty view
 *         if @p cap is an unrecognised enumerator (should never occur in practice).
 */
static constexpr std::string_view capabilityKey(P2PCapability cap) noexcept
{
	switch (cap) {
	case P2PCapability::Read:
		return "r";
	case P2PCapability::Fabric:
		return "n";
	case P2PCapability::Atomics:
		return "a";
	case P2PCapability::Pcie:
		return "p";
	}
	return {};
}

/**
 * @brief Prints the @c --p2p capability list and a usage example to stderr.
 *
 * Called when the user omits or provides an invalid capability argument.
 *
 * @param[in] prog  The program name as invoked (used in the example line).
 */
static void printP2PCapabilityHelp(const std::string &prog)
{
	ERR("--p2p requires a capability argument:\n");
	for (const auto &[key, desc] : P2P_CAP_DESCRIPTIONS) {
		ERR("  {}  {}\n", key, desc);
	}
	ERR("\nExample: {} topology --p2p r\n", prog.c_str());
}

/**
 * @brief Builds the P2P capability matrix for all GPU devices
 * @ingroup topology_matrix
 *
 * Queries Level Zero P2P capabilities between every pair of GPU devices.
 * Operates at device granularity (not tile), matching the Level Zero P2P API contract.
 *
 * @param[in]  args       Pointer to argument structure containing system manager.
 * @param[in]  capability P2P capability to query (see P2PCapability).
 * @param[out] jsonObj    Populated with:
 *                        - @c "p2p":            true (sentinel for printer)
 *                        - @c "p2p_capability": the requested capability letter
 *                        - @c "headers":        GPU label array ("GPU 0", "GPU 1", ...)
 *                        - @c "matrix":         2-D array of per-row {tile, connections}
 *                        - @c "p2p_list":       flat array of per-pair entries
 *                        On error: @c "error" string.
 *
 * @retval ZE_RESULT_SUCCESS           Matrix built successfully
 * @retval ZE_RESULT_ERROR_DEVICE_LOST No devices found
 */
ze_result_t cmdTopology::buildP2PMatrix(arg_struct *args, P2PCapability capability, nlohmann::ordered_json *jsonObj)
{
	TRACING();

	std::vector<devInfo> deviceList;
	const auto result = args->sm.findDevice("", &deviceList);
	if (result != ZE_RESULT_SUCCESS || deviceList.empty()) {
		*jsonObj = {{"error", "No devices found or error getting device list"}};
		return result != ZE_RESULT_SUCCESS ? result : ZE_RESULT_ERROR_DEVICE_LOST;
	}

	const auto deviceCount = deviceList.size();

	// Pre-collect fabric port data only for the "n" capability to avoid redundant
	// ZES calls for the other capability types.
	auto [localPortIds, allPorts] =
		(capability == P2PCapability::Fabric)
			? collectFabricPorts(deviceList)
			: FabricPortData{.localPortIds = std::vector<std::vector<zes_fabric_port_id_t>>(deviceCount),
							 .allPorts = std::vector<std::vector<portInfo>>(deviceCount)};

	std::vector<std::string> headers;
	headers.reserve(deviceCount);
	std::ranges::transform(deviceList, std::back_inserter(headers),
						   [](const devInfo &d) { return std::format("GPU {}", d.index); });

	const std::string capKey{capabilityKey(capability)};

	std::vector<nlohmann::ordered_json> matrixData;
	std::vector<nlohmann::ordered_json> p2pList;

	for (size_t row = 0; row < deviceCount; ++row) {
		nlohmann::ordered_json rowData;
		rowData["tile"] = headers[row];

		std::vector<std::string> connections;
		for (size_t col = 0; col < deviceCount; ++col) {
			if (row == col) {
				connections.emplace_back("X");
				continue;
			}

			const auto supported = queryP2PSupported(capability, deviceList[row].deviceHdl, deviceList[col].deviceHdl,
													 allPorts[row], localPortIds[col]);

			connections.emplace_back(!supported.has_value() ? "?" : (supported.value() ? "OK" : "NS"));

			nlohmann::ordered_json entry;
			entry["source_device_id"] = static_cast<int>(deviceList[row].index);
			entry["target_device_id"] = static_cast<int>(deviceList[col].index);
			entry["capability"] = capKey;
			entry["supported"] =
				supported.has_value() ? nlohmann::ordered_json(supported.value()) : nlohmann::ordered_json(nullptr);
			p2pList.emplace_back(std::move(entry));
		}

		rowData["connections"] = connections;
		matrixData.emplace_back(std::move(rowData));
	}

	(*jsonObj)["p2p"] = true;
	(*jsonObj)["p2p_capability"] = capKey;
	(*jsonObj)["headers"] = headers;
	(*jsonObj)["matrix"] = matrixData;
	(*jsonObj)["p2p_list"] = p2pList;

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Displays the P2P access matrix for all GPU devices
 * @ingroup topology_matrix
 *
 * @param[in] useJson If true, output JSON format; if false, output text table
 *
 * @retval ZE_RESULT_SUCCESS           Matrix displayed successfully
 * @retval ZE_RESULT_ERROR_DEVICE_LOST No devices found in system
 * @retval ZE_RESULT_ERROR_UNINITIALIZED currentArgs not set
 *
 * @see buildP2PMatrix()
 */
ze_result_t cmdTopology::showP2PMatrix(bool useJson, P2PCapability capability)
{
	TRACING();

	if (currentArgs == nullptr) {
		ERR("Internal error: currentArgs not initialized\n");
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	nlohmann::ordered_json jsonObj;
	const auto result = buildP2PMatrix(currentArgs, capability, &jsonObj);

	if (useJson) {
		auto jsonPrinter = std::make_unique<JsonPrinter>();
		jsonPrinter->print(&jsonObj);
	} else {
		auto textPrinter = std::make_unique<TopologyTextPrinter>();
		textPrinter->print(&jsonObj);
	}

	return result;
}

/**
 * @brief Executes the topology command with parsed command line arguments
 * @ingroup topology_commands
 *
 * This is the main entry point for the topology command. It processes command line
 * arguments and dispatches to the appropriate subcommand handler (device query,
 * file generation, or matrix display). Supports multiple output formats (JSON/text)
 * and can handle single or multiple devices.
 *
 * Command line options:
 * - -h, --help: Display help information
 * - -j, --json: Output in JSON format instead of text
 * - --device,--id <id>: Query topology for specific device (by ID or PCI BDF)
 * - -f, --file <path>: Generate topology XML file
 * - -m, --matrix: Display topology connectivity matrix
 *
 * @param[in] args Pointer to argument structure. Must not be nullptr.
 *                 Contains:
 *                 - argc: Argument count
 *                 - argv: Argument vector (command line strings)
 *                 - sm: System manager for device enumeration and queries
 *
 * @retval ZE_RESULT_SUCCESS                   Command executed successfully
 * @retval ZE_RESULT_ERROR_INVALID_ARGUMENT    Invalid command line argument or device not found
 * @retval ZE_RESULT_ERROR_INVALID_NULL_POINTER Device pointer validation failed
 * @retval ZE_RESULT_ERROR_UNINITIALIZED       Internal state not properly initialized
 *
 * @note If no specific command is given, displays help information
 * @note Device queries can be performed on multiple devices matching the criteria
 * @note Sets currentArgs for use by subcommands (generateFile, showMatrix)
 */
int cmdTopology::run(arg_struct *args)
{
	TRACING();

	// Store args for matrix command
	currentArgs = args;

	std::vector<devInfo> deviceList;
	ze_result_t result;

	// Reset all cmd states before parsing
	for (auto &[k, v] : topologyCmds) {
		v.enabled = false;
		v.val.clear();
	}

	CLI::App sub{"Show GPU topology information", "topology"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", topologyCmds[topologyCmdType::TOPOLOGY_JSON].enabled, "Output in JSON format");
	sub.add_option("-d,--device,--id", topologyCmds[topologyCmdType::TOPOLOGY_DEVICE].val,
				   "Device index or BDF address, comma-separated for multiple (e.g. 0,1)")
		->each([&](const std::string &) { topologyCmds[topologyCmdType::TOPOLOGY_DEVICE].enabled = true; });
	sub.add_option("-f,--file", topologyCmds[topologyCmdType::TOPOLOGY_FILE].val, "Output XML file path")
		->each([&](const std::string &val) {
			topologyCmds[topologyCmdType::TOPOLOGY_FILE].enabled = true;
			xmlFilename = val;
		});
	sub.add_flag("-m,--matrix", topologyCmds[topologyCmdType::TOPOLOGY_MATRIX].enabled, "Show topology matrix");
	sub.add_option("--p2p", topologyCmds[topologyCmdType::TOPOLOGY_P2P].val,
				   "P2P capability matrix (r=read, w=write, n=fabric/MDF, a=atomics, p=pcie)")
		->each([&](const std::string &) { topologyCmds[topologyCmdType::TOPOLOGY_P2P].enabled = true; });

	try {
		sub.parse(args->argc - 1, args->argv + 1);
	} catch (const CLI::CallForHelp &) {
		help();
		return ZE_RESULT_SUCCESS;
	} catch (const CLI::ParseError &e) {
		const std::string errMsg = e.what();
		if (errMsg.find("--p2p") != std::string::npos) {
			ERR("{}\n\n", errMsg.c_str());
			printP2PCapabilityHelp(progName);
		} else {
			ERR("{}\n", errMsg.c_str());
			ERR("Run with --help for more information.\n");
		}
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	const bool useJson = topologyCmds[topologyCmdType::TOPOLOGY_JSON].enabled;
	auto textPrinter = std::make_unique<TopologyTextPrinter>();

	// Handle matrix command
	if (topologyCmds[topologyCmdType::TOPOLOGY_MATRIX].enabled) {
		return showMatrix(useJson);
	}

	// Handle P2P matrix command
	if (topologyCmds[topologyCmdType::TOPOLOGY_P2P].enabled) {
		const auto &capStr = topologyCmds[topologyCmdType::TOPOLOGY_P2P].val;
		const auto cap = parseP2PCapability(capStr);
		if (!cap) {
			ERR("Invalid P2P capability '{}'.\n\n", capStr.c_str());
			printP2PCapabilityHelp(progName);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		return showP2PMatrix(useJson, *cap);
	}

	// Handle file generation command
	if (topologyCmds[topologyCmdType::TOPOLOGY_FILE].enabled) {
		const auto &filename = topologyCmds[topologyCmdType::TOPOLOGY_FILE].val;
		return generateFile(filename, useJson);
	}

	// Handle device-specific command
	if (topologyCmds[topologyCmdType::TOPOLOGY_DEVICE].enabled) {
		result = args->sm.findDevice(topologyCmds[topologyCmdType::TOPOLOGY_DEVICE].val.c_str(), &deviceList);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Device handle not found for device ID '{}'.\n",
				topologyCmds[topologyCmdType::TOPOLOGY_DEVICE].val.c_str());
			return result;
		}

		if (deviceList.empty()) {
			ERR("Device not found.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		for (auto &device : deviceList) {
			TopologyInfo info;
			result = showTopology(&device, &info);
			if (result != ZE_RESULT_SUCCESS) {
				return result;
			}

			if (useJson) {
				auto jsonObj = toJson(info);
				auto jsonPrinter = std::make_unique<JsonPrinter>();
				jsonPrinter->print(&jsonObj);
			} else {
				textPrinter->print(info);
			}
		}
		return ZE_RESULT_SUCCESS;
	}

	// No specific command provided, show help
	help();
	return ZE_RESULT_SUCCESS;
}
