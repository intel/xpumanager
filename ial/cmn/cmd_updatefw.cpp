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
#include <fs_lock.h>
#include <debug.h>
#include <assert.h>
#include <thread>
#include <atomic>

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
										"GFX_CODE_DATA, GFX_PSCBIN, AMC, OPCODE, OPDATA."));
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

	// Acquire global firmware update lock (cross-process)
	FSLock fsLock;
	if (!fsLock.locked()) {
		ERR("Another firmware update operation is already in progress or lock could not be acquired.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	uint32_t totalThreads = 0;
	std::atomic<uint32_t> curThread{0};
	std::atomic<ze_result_t> firstError{ZE_RESULT_SUCCESS};
	std::vector<std::thread> workers;
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

	// Print a newline for every thread that we will be creating. Also count the total number of threads
	// as this will come in handy later.
	for (auto &device : deviceList) {
		if ((STRCASECMP(fwInfo.firmwareType.c_str(), "amc") == 0 && device.dev->getAmcIndex() != -1) ||
			STRCASECMP(fwInfo.firmwareType.c_str(), "amc") != 0) {
			PRINT("\n");
			totalThreads++;
		}
	}

	// Parallelize per-device firmware updates
	workers.reserve(totalThreads);

	for (auto &device : deviceList) {
		if ((STRCASECMP(fwInfo.firmwareType.c_str(), "amc") == 0 && device.dev->getAmcIndex() != -1) ||
			STRCASECMP(fwInfo.firmwareType.c_str(), "amc") != 0) {
			workers.emplace_back([&, fwInfo, totalThreads, devPtr = &device]() {
				// Make a thread‑local copy of firmwareInfo to avoid data races
				firmwareInfo localInfo = fwInfo;
				localInfo.dev = devPtr->dev;
				localInfo.deviceIndex = devPtr->index;
				localInfo.amcIndex = devPtr->dev->getAmcIndex();
				localInfo.totalThreads = totalThreads;
				localInfo.curThread = curThread.fetch_add(1, std::memory_order_relaxed);

				firmware *fw = devPtr->dev->getFirmware();
				if (fw == nullptr) {
					ERR("Error: Firmware pointer not found (device %d).\n", devPtr->index);
					ze_result_t expected = ZE_RESULT_SUCCESS;
					firstError.compare_exchange_strong(expected, ZE_RESULT_ERROR_UNKNOWN);
					return;
				}
				if (fw->updateFW(&localInfo) != ZE_RESULT_SUCCESS) {
					ERR("Error: Failed to update firmware for device %d.\n", devPtr->index);
					ze_result_t expected = ZE_RESULT_SUCCESS;
					firstError.compare_exchange_strong(expected, ZE_RESULT_ERROR_UNKNOWN);
					return;
				}
			});
		}
	}

	for (auto &t : workers) {
		if (t.joinable()) {
			t.join();
		}
	}

	if (firstError != ZE_RESULT_SUCCESS) {
		return firstError.load();
	} else {
		if (totalThreads > 0) {
			PRINT("\n"); // Move the cursor to the next line after the last progress bar
		}
		PRINT("Firmware update operation completed successfully.\n");
	}

	return 0;
}
