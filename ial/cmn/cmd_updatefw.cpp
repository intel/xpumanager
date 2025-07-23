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

#include "cmd_updatefw.h"
#include "debug.h"
#include <assert.h>

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdUpdateFW::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Update GPU firmware"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s updatefw [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s updatefw -d [deviceId] -t GFX -f [imageFilePath]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s updatefw -d [pciBdfAddress] -t GFX -f [imageFilePath]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s updatefw -t AMC -f [imageFilePath]", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 The device ID or PCI BDF address. If it is not "
										"specified, all devices will be updated"));
	helpList.push_back(helpCmd(HEADING, "-t,--type                   The firmware name. Valid options: GFX, GFX_DATA, "
										"GFX_CODE_DATA, GFX_PSCBIN, AMC."));
	helpList.push_back(helpCmd(
		SUB_HEADING, "AMC firmware update just works on Intel M50CYP server (BMC firmware version is 2.82 or newer)"));
	helpList.push_back(
		helpCmd(SUB_HEADING, "and Supermicro SYS-620C-TN12R server (BMC firmware version is 11.01 or newer)"));
	helpList.push_back(helpCmd(HEADING, "-f,--file                   The firmware image file path on this server"));
	helpList.push_back(
		helpCmd(HEADING, "-u,--username               Username used to authenticate for host redfish access"));
	helpList.push_back(
		helpCmd(HEADING, "-p,--password               Password used to authenticate for host redfish access"));
	helpList.push_back(helpCmd(
		HEADING, "-y,--assumeyes              Assume that the answer to any question which would be asked is yes"));
	helpList.push_back(helpCmd(
		HEADING, "--force                     Force GFX firmware update. This parameter only works for GFX firmware"));
	helpList.push_back(
		helpCmd(HEADING, "--recovery                  Update firmware under survivability mode. This parameter only "
						 "works for GFX and GFX_DATA firmware on Intel® Data Center GPU Flex series"));

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Executes the updatefw run.
 *
 * @return int Returns 0 on success.
 */
int cmdUpdateFW::run(arg_struct *args)
{
	TRACING();
	firmwareInfo fwInfo = {};
	std::vector<devInfo> deviceList;
	ze_result_t result;

	int opt;
	int optionIndex = 0;
	const char *optString = "hjd:t:f:u:p:y";
	struct option longOpts[] = {{"help", no_argument, nullptr, 'h'},
								{"json", no_argument, nullptr, 'j'},
								{"device", required_argument, nullptr, 'd'},
								{"type", required_argument, nullptr, 't'},
								{"file", required_argument, nullptr, 'f'},
								{"username", required_argument, nullptr, 'u'},
								{"password", required_argument, nullptr, 'p'},
								{"assumeyes", no_argument, nullptr, 'y'},
								{"force", no_argument, nullptr, 0},
								{"recovery", no_argument, nullptr, 0},
								{nullptr, 0, nullptr, 0}};

	// Skip the first two arguments (process and command name)
	int startind = 2;
	optind = 2;

	while ((opt = GETOPT_LONG(args->argc, args->argv, optString, longOpts, &optionIndex)) != -1) {
		switch (opt) {
		case 'h':
			help();
			return ZE_RESULT_SUCCESS;
		case 'j':
			fwInfo.jsonOutput = true;
			break;
		case 'd':
			fwInfo.deviceId = optarg;
			break;
		case 't':
			fwInfo.firmwareType = optarg;
			break;
		case 'f':
			fwInfo.filePath = optarg;
			break;
		case 'u':
			fwInfo.username = optarg;
			break;
		case 'p':
			fwInfo.password = optarg;
			break;
		case 'y':
			fwInfo.assumeYes = true;
			break;
		case 0:
			if (STRCASECMP("force", longOpts[optionIndex].name) == 0) {
				fwInfo.forceUpdate = true;
			} else if (STRCASECMP("recovery", longOpts[optionIndex].name) == 0) {
				fwInfo.recoveryMode = true;
			} else {
				ERR("Unknown command: %s\n", longOpts[optionIndex].name);
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
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

	// Validate firmware type
	if (fwInfo.firmwareType.empty()) {
		ERR("Error: Missing required argument --type.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Validate file path
	if (fwInfo.filePath.empty()) {
		ERR("Error: Missing required argument --file.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	result = args->sm.findDevice(fwInfo.deviceId.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '%s'.\n", fwInfo.deviceId.c_str());
		return result;
	}

	// Iterate through the device list and execute the command
	for (auto &device : deviceList) {
		fwInfo.dev = device.dev;
		fwInfo.deviceHdl = device.deviceHdl;
		firmware *fw = (firmware *)device.dev->getFirmware();
		if (fw == nullptr) {
			ERR("Error: Firmware pointer not found.\n");
			return ZE_RESULT_ERROR_UNKNOWN;
		}

		// Call the hal to update the firmware
		if (fw->updateFW(&fwInfo) != ZE_RESULT_SUCCESS) {
			ERR("Error: Failed to update firmware.\n");
			return ZE_RESULT_ERROR_UNKNOWN;
		}
	}

	if (fwInfo.jsonOutput) {
		// Print JSON output
		INFO("{\"status\": \"success\", \"device\": \"%s\", \"firmware_type\": \"%s\"}\n", fwInfo.deviceId.c_str(),
			 fwInfo.firmwareType.c_str());
	} else {
		// Print human-readable output
		INFO("Firmware update completed successfully for device '%s'.\n", fwInfo.deviceId.c_str());
	}
	return 0;
}
