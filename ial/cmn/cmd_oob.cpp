/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_oob.h"
#include "debug.h"
#include <amclib.h>

/**
 * @brief This structure serves two purposes:
 * 1. It defines the command parsing for oob commands.
 * 2. It allows for easy addition of new commands by simply adding a new entry to the map.
 * oobCmdStruct with a pointer to the function that will be called when the command is executed
 */
static std::unordered_map<oobCmdType, oobCmdStruct> oobCmds = {
	{oobCmdType::OOB_HELP, {{"help", no_argument, 0, 'h'}, false, ""}},
	{oobCmdType::OOB_JSON, {{"json", no_argument, 0, 'j'}, false, ""}},
	{oobCmdType::OOB_DEVICE, {{"device", required_argument, 0, 'd'}, false, ""}},
	{oobCmdType::OOB_IP, {{"ip", required_argument, 0, 'i'}, false, ""}},
	{oobCmdType::OOB_USERNAME, {{"username", required_argument, 0, 'u'}, false, ""}},
	{oobCmdType::OOB_PASSWORD, {{"password", required_argument, 0, 'p'}, false, ""}},
	{oobCmdType::OOB_PORT, {{"port", required_argument, 0, 'o'}, false, ""}}};

/**
 * @brief Displays help information for the out-of-band management command
 *
 * This function shows help text and usage information for OOB management commands.
 *
 * @param helpType Type of help to display (e.g., brief, detailed)
 */
void cmdOOB::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Out-of-Band (OOB) Management Commands"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s oob [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob discovery", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob discovery -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob discovery -d [pciBdfAddress]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s oob discovery -d [deviceId] -j", progName.c_str()));

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Executes the out-of-band management command with specified arguments
 *
 * This function runs OOB management operations based on the provided command line arguments.
 * Currently implemented as a placeholder for future OOB functionality.
 *
 * @param args Pointer to argument structure containing command line arguments (currently unused)
 * @return int Returns 0 on success, non-zero on failure
 */
int cmdOOB::run(UNUSED arg_struct *args)
{
	TRACING();
	int opt;
	int optionIndex = 0;
	std::string shortOpts;
	std::vector<struct option> longOptsVec;
	amclib amcobj;
	uint16_t portNum = 0;
	RedfishGPUDevice *gpuDevices = nullptr;
	int foundCount = 0;
	bool found = false;

	processOptions(oobCmds, shortOpts, longOptsVec);
	const struct option *longOpts = longOptsVec.data();

	// Skip the first two arguments (process and command name)
	int startind = 2;
	optind = 2;

	while ((opt = getopt_long(args->argc, args->argv, shortOpts.c_str(), longOpts, &optionIndex)) != -1) {
		switch (opt) {
		case 'h':
			help();
			return 0;
		case 'j':
			oobCmds[oobCmdType::OOB_JSON].enabled = true;
			break;
		case 'd':
			oobCmds[oobCmdType::OOB_DEVICE].enabled = true;
			oobCmds[oobCmdType::OOB_DEVICE].val = optarg;
			break;
		case 'i':
			oobCmds[oobCmdType::OOB_IP].enabled = true;
			oobCmds[oobCmdType::OOB_IP].val = optarg;
			break;
		case 'u':
			oobCmds[oobCmdType::OOB_USERNAME].enabled = true;
			oobCmds[oobCmdType::OOB_USERNAME].val = optarg;
			break;
		case 'p':
			oobCmds[oobCmdType::OOB_PASSWORD].enabled = true;
			oobCmds[oobCmdType::OOB_PASSWORD].val = optarg;
			break;
		case 'o':
			try {
				oobCmds[oobCmdType::OOB_PORT].enabled = true;
				oobCmds[oobCmdType::OOB_PORT].val = optarg;
				portNum = static_cast<uint16_t>(std::stoi(optarg));
			} catch (const std::exception &) {
				ERR("Invalid port number: '%s'.\n", optarg);
			}
			break;
		default:
			ERR("The following argument was not expected: '%s'.\n", args->argv[startind]);
			ERR("Run with --help for more information.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
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

	PRINT("OOB discovery can take up to a minute to complete.\r");
	fflush(stdout);

	// Make sure that ip, username and password are always provided else return an error
	if (!oobCmds[oobCmdType::OOB_IP].enabled || !oobCmds[oobCmdType::OOB_USERNAME].enabled ||
		!oobCmds[oobCmdType::OOB_PASSWORD].enabled || !oobCmds[oobCmdType::OOB_PORT].enabled) {
		// Clear the "OOB discovery" message
		PRINT("\r%*s\r", 80, "");
		ERR("IP, username, password, and port must be provided.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (amcobj.redfishInitialize(oobCmds[oobCmdType::OOB_IP].val, oobCmds[oobCmdType::OOB_USERNAME].val,
								 oobCmds[oobCmdType::OOB_PASSWORD].val, portNum)) {
		// Clear the "OOB discovery" message
		PRINT("\r%*s\r", 80, "");
		ERR("Failed to initialize Redfish.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	if (amcobj.redfishDiscovery(&gpuDevices, &foundCount)) {
		// Clear the "OOB discovery" message
		PRINT("\r%*s\r", 80, "");
		ERR("Failed to discover GPU devices.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	// Clear the "OOB discovery" message
	PRINT("\r%*s\r", 80, "");

	for (int i = 0; i < foundCount; i++) {
		// If either the device parameter wasn't provided or this device is the device index, then print the
		// corresponding info
		if (!oobCmds[oobCmdType::OOB_DEVICE].enabled ||
			oobCmds[oobCmdType::OOB_DEVICE].val == std::to_string(gpuDevices[i].id) ||
			oobCmds[oobCmdType::OOB_DEVICE].val == gpuDevices[i].pciBdfAddress) {
			found = true;
			PRINT("  Name: %s\n", gpuDevices[i].deviceName.c_str());
			PRINT("  Vendor: %s\n", gpuDevices[i].vendorName.c_str());
			PRINT("  Device ID: %s\n", gpuDevices[i].deviceId.c_str());
			PRINT("  Vendor ID: %s\n", gpuDevices[i].vendorId.c_str());
			PRINT("  Class Code: %s\n", gpuDevices[i].classCode.c_str());
			PRINT("  Device Class: %s\n", gpuDevices[i].deviceClass.c_str());
			PRINT("  PCI BDF Address: %s\n", gpuDevices[i].pciBdfAddress.c_str());
			PRINT("  Function Type: %s\n", gpuDevices[i].functionType.c_str());
			PRINT("  device_path = %s\n", gpuDevices[i].devicePath.c_str());
			PRINT("  function_endpoint = %s\n", gpuDevices[i].functionEndpoint.c_str());
		}
	}

	if (!found) {
		ERR("No matching GPU device found.\n");
	}

	amcobj.freeGpuDevices(gpuDevices);

	return 0;
}