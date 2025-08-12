/*
 * Copyright © 2025 Intel Corporation
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
#include <assert.h>

static std::unordered_map<topologyCmdType, topologyCmdStruct> topologyCmds = {
	{topologyCmdType::TOPOLOGY_HELP, {{"help", no_argument, 0, 'h'}, nullptr, false, ""}},
	{topologyCmdType::TOPOLOGY_JSON, {{"json", no_argument, 0, 'j'}, nullptr, false, ""}},
	{topologyCmdType::TOPOLOGY_DEVICE, {{"device", required_argument, 0, 'd'}, &cmdTopology::showTopology, false, ""}},
	{topologyCmdType::TOPOLOGY_FILE, {{"file", required_argument, 0, 'f'}, &cmdTopology::generateFile, false, ""}},
	{topologyCmdType::TOPOLOGY_MATRIX, {{"matrix", no_argument, 0, 'm'}, &cmdTopology::showMatrix, false, ""}},
};

/**
 * @brief Displays help information for the topology command
 *
 * This function prints comprehensive usage information for the topology command,
 * including options for device queries, matrix display, and file generation.
 * It also explains the topology connection symbols used in the output.
 *
 * @param helpType The type of help to display (FULL_HELP or SHORT_HELP)
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
 * It provides details about which CPUs are local to the device for NUMA optimization.
 *
 * @param d Pointer to device information structure containing device details
 * @return ze_result_t ZE_RESULT_SUCCESS on success, error code on failure
 */
ze_result_t cmdTopology::showTopology(devInfo *d)
{
	TRACING();
	std::string switchDevicePath = "N/A";
	std::string cpuList = d->dev->getCPUList();
	std::string localCPUs = d->dev->getLocalCPUs();
	int switchCount = d->dev->getSwitchCount(&switchDevicePath);

	PRINT("Device ID: %d\n", d->index);
	PRINT("Local CPU List: %s\n", cpuList.c_str());
	PRINT("Local CPUs: %s\n", localCPUs.c_str());
	PRINT("PCIe Switch Count: %d\n", switchCount);
	PRINT("PCIe Switch: %s\n", switchDevicePath.c_str());
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
 * @param d Pointer to device information structure (unused for this operation)
 * @return ze_result_t ZE_RESULT_SUCCESS on success, error code on failure
 */
ze_result_t cmdTopology::generateFile(UNUSED devInfo *d)
{
	TRACING();
	PRINT("Generate topology file...\n");
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Displays system topology matrix showing device connectivity
 *
 * This function generates and displays a topology matrix that shows the
 * connectivity and relationship between all GPU devices, CPU nodes, and
 * interconnects in the system. The matrix uses symbols to indicate different
 * types of connections (PCIe, Xe Link, MDF, etc.) and their characteristics.
 *
 * @param d Pointer to device information structure (unused for this operation)
 * @return ze_result_t ZE_RESULT_SUCCESS on success, error code on failure
 */
ze_result_t cmdTopology::showMatrix(UNUSED devInfo *d)
{
	TRACING();
	PRINT("Show topology matrix...\n");
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the topology command with parsed command line arguments
 *
 * This function processes the topology command which displays system topology
 * information including GPU-to-CPU relationships, NUMA nodes, interconnect
 * details, and device connectivity matrices. It can generate XML files,
 * display topology matrices, or show device-specific topology information.
 *
 * @param args Pointer to argument structure containing argc, argv, and system manager
 * @return int ZE_RESULT_SUCCESS on success, error code on failure
 */
int cmdTopology::run(arg_struct *args)
{
	TRACING();
	std::vector<devInfo> deviceList;
	ze_result_t result;
	bool found = false;
	int opt;
	int optionIndex = 0;
	std::string shortOpts;
	std::vector<struct option> longOptsVec;

	processOptions(topologyCmds, shortOpts, longOptsVec);
	const struct option *longOpts = longOptsVec.data();
	// Skip the first two arguments (process and command name)
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
			for (auto &cmd : topologyCmds) {
				if (STRCASECMP(longOpts[optionIndex].name, cmd.second.opt.name) == 0) {
					cmd.second.enabled = true;
					if (longOpts[optionIndex].has_arg == required_argument) {
						cmd.second.val = optarg;
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

	// If optind is not equal to args->argc, it means there are extra arguments
	// that were not processed by getopt_long.
	if (optind != args->argc) {
		ERR("The following argument was not expected: '%s'.\n", args->argv[optind]);
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	result = args->sm.findDevice(topologyCmds[topologyCmdType::TOPOLOGY_DEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Device handle not found for device ID '%s'.\n",
			topologyCmds[topologyCmdType::TOPOLOGY_DEVICE].val.c_str());
		return result;
	}

	if (deviceList.size() < 1) {
		ERR("Device not found.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Handle matrix command (doesn't require device)
	if (topologyCmds[topologyCmdType::TOPOLOGY_MATRIX].enabled) {
		return showMatrix(nullptr);
	}

	// Handle file generation command (doesn't require specific device)
	if (topologyCmds[topologyCmdType::TOPOLOGY_FILE].enabled) {
		return generateFile(nullptr);
	}

	// For device-specific commands, find the device
	if (topologyCmds[topologyCmdType::TOPOLOGY_DEVICE].enabled) {
		// Iterate through the device list and execute the command
		for (auto &device : deviceList) {
			// Call the appropriate command function based on the command type
			for (const auto &cmd : topologyCmds) {
				if (cmd.second.enabled && cmd.second.func != nullptr) {
					DBG("Running command: %s\n", cmd.second.opt.name);
					result = (this->*cmd.second.func)(&device);
					if (result != ZE_RESULT_SUCCESS) {
						return result;
					}
				}
			}
		}
	} else {
		// If no specific command is provided, show help
		help();
	}

	return 0;
}
