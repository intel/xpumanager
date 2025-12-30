/*
 * Copyright © 2025-2026 Intel Corporation
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

#include "cmd_topology.h"
#include "debug.h"
#include "printer.h"
#include <assert.h>
#include <format>

/**
 * @brief Constructor for TopologyTextPrinter
 */
TopologyTextPrinter::TopologyTextPrinter() : TextPrinter() {}

/**
 * @brief Custom text printer for topology command output (legacy JSON support)
 *
 * This function formats the JSON object containing topology data into
 * human-readable text format with proper labels and formatting. Supports
 * both simple message output and detailed device topology information.
 *
 * @param[in] jsonObj Pointer to the JSON object containing topology data.
 *                    Must contain either a "message" field or device topology fields
 *                    (device_id, local_cpu_list, local_cpus, pcie_switch_count, pcie_switch).
 *                    Must not be nullptr.
 *
 * @note Outputs directly to stdout using PRINT macro
 * @note Handles two formats:
 *       - Simple message: {"message": "..."}
 *       - Device topology: {"device_id": N, "local_cpu_list": "...", ...}
 */
void TopologyTextPrinter::print(nlohmann::ordered_json *jsonObj)
{
	TRACING();

	// Check if this is a simple message (for generateFile or showMatrix)
	if (const auto msg = jsonObj->value("message", ""); !msg.empty()) {
		PRINT("%s", std::format("{}\n", msg).c_str());
		return;
	}

	// Format device topology information using std::format for type-safe formatting
	if (const auto deviceId = jsonObj->value("device_id", -1); deviceId != -1) {
		PRINT("%s", std::format("Device ID: {}\n", deviceId).c_str());
	}
	if (const auto cpuList = jsonObj->value("local_cpu_list", std::string{}); !cpuList.empty()) {
		PRINT("%s", std::format("Local CPU List: {}\n", cpuList).c_str());
	}
	if (const auto localCpus = jsonObj->value("local_cpus", std::string{}); !localCpus.empty()) {
		PRINT("%s", std::format("Local CPUs: {}\n", localCpus).c_str());
	}
	if (jsonObj->contains("pcie_switch_count")) {
		const auto count = jsonObj->value("pcie_switch_count", 0);
		PRINT("%s", std::format("PCIe Switch Count: {}\n", count).c_str());
	}
	if (const auto pcieSwitch = jsonObj->value("pcie_switch", std::string{}); !pcieSwitch.empty()) {
		PRINT("%s", std::format("PCIe Switch: {}\n", pcieSwitch).c_str());
	}
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
	helpList.push_back(helpCmd(SUB_HEADING, "XL[laneCount]: Two tiles on the different cards are directly connected by "
											"Xe Link.  Xe Link LAN count is also provided"));
	helpList.push_back(helpCmd(SUB_HEADING, "XL*: Two tiles on the different cards are connected by Xe Link + MDF. "
											"They are not directly connected by Xe Link"));
	helpList.push_back(helpCmd(SUB_HEADING, "SYS: Connected with PCIe between NUMA nodes"));
	helpList.push_back(helpCmd(SUB_HEADING, "NODE: Connected with PCIe within a NUMA node"));
	helpList.push_back(helpCmd(SUB_HEADING, "MDF: Connected with Multi-Die Fabric Interface"));

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Displays topology information for a specified device
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
 * @note This information is critical for NUMA-aware workload placement
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
 *
 * This function creates an XML file containing comprehensive system topology
 * information including GPU devices, CPU nodes, interconnect details, and
 * NUMA relationships. The generated file can be used for system analysis
 * and topology visualization tools.
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
 * @note Currently prints placeholder message. Actual XML topology generation is TODO.
 * @note Output is printed directly to stdout
 * @note Requires currentArgs to be set (automatically done by run() method)
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
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	// TODO: Implement actual topology file generation
	const std::string message = std::format("Topology exported to file: {}", filename);

	if (useJson) {
		auto jsonObj = nlohmann::ordered_json{{"filename", filename}, {"message", message}};
		auto printer = std::make_unique<JsonPrinter>();
		printer->print(&jsonObj);
	} else {
		PRINT("%s\n", message.c_str());
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Displays system topology matrix showing device connectivity
 *
 * This function generates and displays a topology matrix that shows the
 * connectivity and relationship between all GPU devices, CPU nodes, and
 * interconnects in the system. The matrix uses symbols to indicate different
 * types of connections:
 * - S: Self (same device)
 * - XL[N]: Xe Link with N lanes
 * - XL*: Xe Link + MDF (not direct)
 * - SYS: PCIe between NUMA nodes
 * - NODE: PCIe within NUMA node
 * - MDF: Multi-Die Fabric Interface
 *
 * @param[in] useJson Output format selector:
 *                    - true: Print matrix as JSON: {"message": "..."}
 *                    - false: Print matrix as plain text
 *
 * @retval ZE_RESULT_SUCCESS             Matrix generation and display successful
 * @retval ZE_RESULT_ERROR_UNINITIALIZED currentArgs is nullptr (internal error)
 *
 * @note Currently prints placeholder message. Actual matrix generation is TODO.
 * @note Output is printed directly to stdout
 * @note Requires currentArgs to be set (automatically done by run() method)
 * @note The matrix helps identify optimal device placement for multi-GPU workloads
 */
ze_result_t cmdTopology::showMatrix(bool useJson)
{
	TRACING();

	if (currentArgs == nullptr) {
		ERR("Internal error: args not available\n");
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	// TODO: Implement actual topology matrix generation
	const std::string message = "Show topology matrix...";

	if (useJson) {
		auto jsonObj = nlohmann::ordered_json{{"message", message}};
		auto printer = std::make_unique<JsonPrinter>();
		printer->print(&jsonObj);
	} else {
		PRINT("%s\n", message.c_str());
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the topology command with parsed command line arguments
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

	// Store args for commands that need it
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
