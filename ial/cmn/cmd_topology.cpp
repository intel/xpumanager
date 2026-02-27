/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_topology.h"
#include "debug.h"
#include "printer.h"
#include "table_builder.h"
#include <assert.h>
#include <format>
#include <algorithm>
#include <ranges>
#include <concepts>
#include <optional>
#include <os.h>

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
TopologyTextPrinter::TopologyTextPrinter() : TextPrinter() {}

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
		PRINT("%s", std::format("Error: {}\n", jsonObj->value("error", "")).c_str());
		return;
	}

	if (const auto msg = jsonObj->value("message", ""); !msg.empty()) {
		PRINT("%s", std::format("{}\n", msg).c_str());
		return;
	}

	if (jsonObj->contains("matrix") && jsonObj->contains("headers")) {
		const auto &headers = (*jsonObj)["headers"];
		const auto &matrix = (*jsonObj)["matrix"];

		TableBuilder table;
		table.addColumn("", MATRIX_TILE_COL_WIDTH);
		for (const auto &header : headers) {
			const auto &name = header.get<std::string>();
			const int colWidth = std::max(MATRIX_CONNECTION_COL_WIDTH, static_cast<int>(name.size()));
			table.addColumn(name, colWidth);
		}

		// Compute affinity column width from actual data to avoid truncation
		int affinityWidth = MATRIX_AFFINITY_COL_WIDTH;
		for (const auto &row : matrix) {
			const auto affinity = row["cpu_affinity"].get<std::string>();
			affinityWidth = std::max(affinityWidth, static_cast<int>(affinity.size()));
		}
		table.addColumn("CPU Affinity", affinityWidth);

		for (const auto &row : matrix) {
			std::vector<std::string> cells;
			cells.push_back(row["tile"].get<std::string>());
			for (const auto &conn : row["connections"]) {
				cells.push_back(conn.get<std::string>());
			}
			cells.push_back(row["cpu_affinity"].get<std::string>());
			table.addRowFromContainer(cells);
		}

		PRINT("%s", table.toString().c_str());
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

	PRINT("%s", table.toString().c_str());
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

	PRINT("%s", table.toString().c_str());
}

static std::unordered_map<topologyCmdType, TopologyCmdStruct> topologyCmds = {
	{topologyCmdType::TOPOLOGY_HELP, {{"help", no_argument, 0, 'h'}, false, ""}},
	{topologyCmdType::TOPOLOGY_JSON, {{"json", no_argument, 0, 'j'}, false, ""}},
	{topologyCmdType::TOPOLOGY_DEVICE, {{"device", required_argument, 0, 'd'}, false, ""}},
	{topologyCmdType::TOPOLOGY_FILE, {{"file", required_argument, 0, 'f'}, false, ""}},
	{topologyCmdType::TOPOLOGY_MATRIX, {{"matrix", no_argument, 0, 'm'}, false, ""}},
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

	helpList.push_back(helpCmd(TITLE, "Get the system topology"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s topology [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s topology -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s topology -d [pciBdfAddress]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s topology -d [deviceId] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s topology -f [filename]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s topology -m", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 The device ID or PCI BDF address to query"));
	helpList.push_back(
		helpCmd(HEADING, "-f,--file                   Generate the system topology with the GPU info to a XML file"));
	helpList.push_back(helpCmd(HEADING, "-m,--matrix                 Print the CPU/GPU topology matrix"));
	helpList.push_back(helpCmd(SUB_HEADING, "S: Self"));
	helpList.push_back(helpCmd(SUB_HEADING, "MDF: Connected with Multi-Die Fabric Interface"));
	helpList.push_back(helpCmd(SUB_HEADING, "NODE: Connected with PCIe within a NUMA node"));
	helpList.push_back(helpCmd(SUB_HEADING, "SYS: Connected with PCIe between NUMA nodes"));

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
		ERR("Failed to export topology to file '%s'\n", filename.c_str());
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	const std::string message = std::format("Topology exported to file: {}", filename);

	if (useJson) {
		auto jsonObj = std::make_unique<nlohmann::ordered_json>();
		(*jsonObj)["message"] = message;
		auto printer = std::make_unique<JsonPrinter>();
		printer->print(jsonObj.get());
	} else {
		PRINT("%s\n", message.c_str());
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Builds the topology connectivity matrix for all devices
 * @ingroup topology_matrix
 *
 * Constructs a comprehensive topology matrix showing connectivity between all GPU tiles
 * in the system. Examines PCIe topology and CPU affinity to determine link types (MDF, PCIe).
 * Generates both a flat topology list for JSON output and a 2D matrix for text display.
 *
 * @param[in]  args    Pointer to argument structure containing system manager for device queries.
 *                     Must not be nullptr. The args->sm field must be valid.
 * @param[out] jsonObj Pointer to JSON object that will be populated with topology data.
 *                     Must not be nullptr. On success, contains:
 *                     - "topo_list": Flat array of topology entries for JSON compatibility
 *                     - "headers": Array of tile labels (e.g., "GPU 0/0", "GPU 0/1")
 *                     - "matrix": 2D array of connection data for text printing
 *                     On error, contains:
 *                     - "error": Error message string
 *
 * @retval ZE_RESULT_SUCCESS           Matrix built successfully
 * @retval ZE_RESULT_ERROR_DEVICE_LOST No devices found in the system
 * @retval Other                       Device enumeration failed (from findDevice)
 *
 * @pre args and args->sm must be initialized and valid
 * @pre jsonObj must point to valid nlohmann::ordered_json object
 *
 * @post On success, jsonObj contains complete topology matrix data
 * @post allTiles vector contains all tile information
 *
 * @note Link type determination:
 *       - "S": Self (same tile)
 *       - "MDF": Multi-Die Fabric (same device, different tiles)
 *       - "NODE": PCIe within same NUMA node
 *       - "SYS": PCIe across NUMA nodes
 * @note Matrix is NxN where N = total number of tiles across all devices
 * @note CPU affinity determines NUMA node relationships
 *
 * @see determineLinkType() for link type determination algorithm
 * @see TileInfo structure for tile metadata
 */
ze_result_t cmdTopology::buildTopologyMatrix(arg_struct *args, nlohmann::ordered_json *jsonObj)
{
	TRACING();
	// Get all devices from system manager
	std::vector<devInfo> deviceList;
	const auto result = args->sm.findDevice("", &deviceList);
	if (result != ZE_RESULT_SUCCESS || deviceList.empty()) {
		*jsonObj = {{"error", "No devices found or error getting device list"}};
		return result != ZE_RESULT_SUCCESS ? result : ZE_RESULT_ERROR_DEVICE_LOST;
	}

	// Collect all tiles across all devices
	std::vector<TileInfo> allTiles;
	std::vector<std::string> deviceHeaders;
	std::map<int, bool> deviceHasSubdevices; // Track which devices have subdevices

	for (auto &device : deviceList) {
		const auto cpuAffinity = device.dev->getCPUList(); // Use getCPUList() for range format like "0-31"
		std::map<uint32_t, std::vector<portInfo>> portsByTile;

		// It doesn't mean "has multiple tiles", it means "this is a tile-level view"
		const bool hasSubdevices = true;

		fabric *f = device.dev->getFabric();
		if (f != nullptr) {
			std::vector<portInfo> portInfoList;
			const auto fabricResult = f->getFabricPorts(device.zesDeviceHdl, portInfoList);

			if (fabricResult == ZE_RESULT_SUCCESS) {
				// Group ports by subdevice/tile
				for (const auto &pi : portInfoList) {
					if (pi.portProps.onSubdevice) {
						portsByTile[pi.portProps.subdeviceId].push_back(pi);
					} else {
						portsByTile[0].push_back(pi);
					}
				}
			}
		}

		deviceHasSubdevices[device.index] = hasSubdevices;

		// If no fabric ports or fabric not available, add device with single tile
		if (portsByTile.empty()) {
			TileInfo tile{
				.deviceId = static_cast<int>(device.index), .tileId = 0, .cpuAffinity = cpuAffinity, .ports = {}};
			allTiles.push_back(tile);
			deviceHeaders.push_back(std::format("GPU {}/0", device.index));
		} else {
			// Create tile entries with fabric port information
			for (const auto &[tileId, ports] : portsByTile) {
				TileInfo tile{.deviceId = static_cast<int>(device.index),
							  .tileId = static_cast<int>(tileId),
							  .cpuAffinity = cpuAffinity,
							  .ports = ports};
				allTiles.push_back(tile);
				deviceHeaders.push_back(std::format("GPU {}/{}", device.index, tileId));
			}
		}
	}

	// Build topology list (for JSON) and matrix (for text display)
	const auto tileCount = allTiles.size();
	std::vector<nlohmann::ordered_json> topoList;
	std::vector<nlohmann::ordered_json> matrixData;

	for (const auto row : std::views::iota(size_t{0}, tileCount)) {
		nlohmann::ordered_json rowData;
		rowData["tile"] = deviceHeaders[row];
		rowData["cpu_affinity"] = allTiles[row].cpuAffinity;

		std::vector<std::string> connections;
		for (const auto col : std::views::iota(size_t{0}, tileCount)) {
			std::string linkType;
			if (row == col) {
				linkType = "S";
			} else {
				linkType = determineLinkType(allTiles[row], allTiles[col]);
			}
			connections.push_back(linkType);

			// Add to flat topology list for JSON output
			nlohmann::ordered_json topoEntry;
			topoEntry["link_type"] = linkType;
			topoEntry["local_cpu_affinity"] = allTiles[row].cpuAffinity;
			topoEntry["local_device_id"] = allTiles[row].deviceId;
			topoEntry["local_numa_index"] = 0; // Would need proper NUMA detection
			topoEntry["local_on_subdevice"] = deviceHasSubdevices[allTiles[row].deviceId];
			topoEntry["local_subdevice_id"] = allTiles[row].tileId;
			topoEntry["max_bit_rate"] = -1; // Would need to calculate from port info
			topoEntry["remote_device_id"] = allTiles[col].deviceId;
			topoEntry["remote_subdevice_id"] = allTiles[col].tileId;
			topoList.push_back(topoEntry);
		}
		rowData["connections"] = connections;
		matrixData.push_back(rowData);
	}

	// Build final JSON structure - use old format for compatibility
	(*jsonObj)["topo_list"] = topoList;

	// Store matrix data for text printer
	(*jsonObj)["headers"] = deviceHeaders;
	(*jsonObj)["matrix"] = matrixData;

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Determines the link type between two GPU tiles
 * @ingroup topology_matrix
 *
 * Analyzes the connectivity between two tiles by examining device topology
 * and CPU affinity. Uses a hierarchical decision process to classify the
 * connection type.
 *
 * Algorithm:
 * 1. Check if same tile → Self (S)
 * 2. Check if same device but different tiles → MDF
 * 3. For PCIe connections, compare CPU affinity:
 *    - Same affinity → NODE (within NUMA node)
 *    - Different affinity → SYS (across NUMA nodes)
 *
 * @param[in] tile1 First tile information. Contains:
 *                  - deviceId: Device index
 *                  - tileId: Tile/subdevice index within device
 *                  - cpuAffinity: CPU list string (e.g., "0-31")
 *                  - ports: Vector of fabric port information (unused)
 * @param[in] tile2 Second tile information. Same structure as tile1.
 *
 * @return std::string Link type symbol:
 *         - "S": Self (same tile)
 *         - "MDF": Multi-Die Fabric Interface (same device, different tiles)
 *         - "NODE": PCIe within same NUMA node
 *         - "SYS": PCIe across NUMA nodes
 *
 * @pre tile1 and tile2 must contain valid device and tile IDs
 *
 * @note CPU affinity comparison is simplified - full NUMA parsing would be more accurate
 * @note This is a const method as it only reads tile information
 *
 * @see buildTopologyMatrix() for usage context
 * @see TileInfo structure definition
 */
std::string cmdTopology::determineLinkType(const TileInfo &tile1, const TileInfo &tile2) const
{
	TRACING();
	// Same tile - Self
	if (tile1.deviceId == tile2.deviceId && tile1.tileId == tile2.tileId) {
		return "S";
	}

	// Same device, different tiles - MDF (Multi-Die Fabric)
	if (tile1.deviceId == tile2.deviceId && tile1.tileId != tile2.tileId) {
		return "MDF";
	}

	// PCIe connections - check if same NUMA node
	// Simplified: would need to parse CPU affinity to determine NUMA
	if (tile1.cpuAffinity == tile2.cpuAffinity) {
		return "NODE";
	}

	return "SYS";
}

/**
 * @brief Displays the topology connectivity matrix for all GPU devices
 * @ingroup topology_matrix
 *
 * Generates and displays a comprehensive topology matrix showing connectivity
 * between all GPU tiles in the system. The matrix shows link types between
 * each pair of tiles, including MDF and PCIe connections.
 *
 * Output format depends on useJson parameter:
 * - JSON mode: Outputs structured JSON with topo_list and matrix arrays
 * - Text mode: Displays formatted table with headers and connection symbols
 *
 * @param[in] useJson If true, output JSON format; if false, output text table
 *
 * @retval ZE_RESULT_SUCCESS           Matrix displayed successfully
 * @retval ZE_RESULT_ERROR_DEVICE_LOST No devices found in system
 * @retval ZE_RESULT_ERROR_UNINITIALIZED currentArgs not set (run() must be called first)
 *
 * @pre currentArgs must be set by run() before calling this function
 * @post Matrix is printed to stdout via appropriate printer
 *
 * @note Uses buildTopologyMatrix() to generate the topology data
 * @note Connection symbols: S (self), MDF, NODE, SYS
 * @note Requires at least one device in the system
 *
 * @see buildTopologyMatrix() for matrix generation algorithm
 * @see determineLinkType() for link type classification
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
 * - -d, --device <id>: Query topology for specific device (by ID or PCI BDF)
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
 * @note Uses getopt_long for command line parsing
 * @note Sets currentArgs for use by subcommands (generateFile, showMatrix)
 */
int cmdTopology::run(arg_struct *args)
{
	TRACING();

	// Store args for matrix command
	currentArgs = args;

	std::vector<devInfo> deviceList;
	ze_result_t result;
	bool found = false;
	int opt;
	int optionIndex = 0;
	std::string shortOpts;
	std::vector<struct option> longOptsVec;

	processOptions(topologyCmds, shortOpts, longOptsVec);
	const struct option *longOpts = longOptsVec.data();
	int startind = 2;
	optind = 2;

	while ((opt = getopt_long(args->argc, args->argv, shortOpts.c_str(), longOpts, &optionIndex)) != -1) {
		switch (opt) {
		case 'h':
			help();
			return ZE_RESULT_SUCCESS;
		case 'j':
			topologyCmds[topologyCmdType::TOPOLOGY_JSON].enabled = true;
			break;
		case 'd':
			topologyCmds[topologyCmdType::TOPOLOGY_DEVICE].val = optarg;
			topologyCmds[topologyCmdType::TOPOLOGY_DEVICE].enabled = true;
			break;
		case 'f':
			topologyCmds[topologyCmdType::TOPOLOGY_FILE].val = optarg;
			topologyCmds[topologyCmdType::TOPOLOGY_FILE].enabled = true;
			xmlFilename = optarg; // Store filename for use in generateFile()
			break;
		case 'm':
			topologyCmds[topologyCmdType::TOPOLOGY_MATRIX].enabled = true;
			break;
		case 0:
			for (auto &[cmdType, cmdStruct] : topologyCmds) {
				if (STRCASECMP(longOpts[optionIndex].name, cmdStruct.opt.name) == 0) {
					cmdStruct.enabled = true;
					if (longOpts[optionIndex].has_arg == required_argument) {
						cmdStruct.val = optarg;
					}
					found = true;
					break;
				}
			}
			if (!found) {
				ERR("The following argument was not expected: '%s'.\n", longOpts[optionIndex].name);
				ERR("Run with --help for more information.\n");
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}
			break;
		default:
			ERR("The following argument was not expected: '%s'.\n", args->argv[startind]);
			ERR("Run with --help for more information.\n");
			break;
		}
		startind++;
	}

	if (optind != args->argc) {
		ERR("The following argument was not expected: '%s'.\n", args->argv[optind]);
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	const bool useJson = topologyCmds[topologyCmdType::TOPOLOGY_JSON].enabled;
	auto textPrinter = std::make_unique<TopologyTextPrinter>();

	// Handle matrix command
	if (topologyCmds[topologyCmdType::TOPOLOGY_MATRIX].enabled) {
		return showMatrix(useJson);
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
			ERR("Device handle not found for device ID '%s'.\n",
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
