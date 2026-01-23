/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_amc.h"
#include "debug.h"
#include "amclib.h"
#include <atomic>
#include <thread>
#include <vector>
#include <iostream>
#include <unordered_map>

static std::unordered_map<amcSubCmdType, amcSubCmdStruct> amcCmds = {
	{AMC_HELP, {{"help", no_argument, 0, 'h'}, nullptr, false, ""}},
	{AMC_GPURESET, {{"gpureset", no_argument, 0, 0}, &cmdAmc::gpuReset, false, ""}},
	{AMC_DEVICE, {{"device", required_argument, 0, 'd'}, nullptr, false, ""}},
	{AMC_YES, {{"yes", no_argument, 0, 'y'}, nullptr, false, ""}},
};
/**
 * @brief Displays help information for the AMC command
 *
 * This function generates and displays comprehensive help information for the
 * AMC command, including usage examples,
 * available operations, and command descriptions for GPU management via AMC.
 *
 * @param[in] helpType Type of help to display (SHORT_HELP for brief, FULL_HELP for detailed)
 */
void cmdAmc::help(HELP helpType)
{
	std::vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "AMC Operations"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s amc [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amc --gpureset", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amc --gpureset -y", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amc --gpureset -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amc --gpureset -d [deviceId] -y", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "--gpureset                  Reset GPU(s) via AMC"));
	helpList.push_back(helpCmd(
		HEADING,
		"-d,--device                 Specify the device ID. If not specified, operation applies to all devices"));
	helpList.push_back(helpCmd(HEADING, "-y,--yes                    Skip confirmation prompt"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "WARNING:"));
	helpList.push_back(helpCmd(HEADING, "  - This operation may require root/administrator privileges"));

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Executes the AMC command with parsed arguments
 *
 * This function implements the main execution logic for the AMC command,
 * parsing command line arguments, validating operations, and dispatching to
 * the appropriate handler function. It initializes the AMC library and manages
 * the overall command flow.
 *
 * @param[in] args Pointer to argument structure containing command line parameters
 * @return int Return code: ZE_RESULT_SUCCESS on success, error code on failure
 */
int cmdAmc::run(arg_struct *args)
{
	TRACING();
	std::string shortOpts;
	std::vector<struct option> longOptsVec;

	processOptions(amcCmds, shortOpts, longOptsVec);
	const struct option *longOpts = longOptsVec.data();

	int opt;
	int optionIndex = 0;

	int startind = 2;
	optind = 2;

	if (args->argc == 2) {
		help();
		return ZE_RESULT_SUCCESS;
	}

	while ((opt = getopt_long(args->argc, args->argv, shortOpts.c_str(), longOpts, &optionIndex)) != -1) {
		switch (opt) {
		case 'h':
			help();
			return 0;
		case 'd':
			amcCmds[AMC_DEVICE].enabled = true;
			amcCmds[AMC_DEVICE].val = optarg;
			break;
		case 'y':
			amcCmds[AMC_YES].enabled = true;
			break;
		case 0: {
			bool found = false;
			for (auto &cmd : amcCmds) {
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
		}
		default:
			ERR("The following argument was not expected: '%s'.\n", args->argv[startind]);
			ERR("Run with --help for more information.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		startind++;
	}

	if (optind != args->argc) {
		ERR("The following argument was not expected: '%s'.\n", args->argv[optind]);
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Validate that only one functional operation is specified
	int operationCount = 0;
	for (const auto &entry : amcCmds) {
		if (entry.second.enabled && entry.second.func != nullptr) {
			operationCount++;
		}
	}

	if (operationCount > 1) {
		ERR("Error: Only one AMC operation can be specified at a time.\n");
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (operationCount == 0) {
		help(SHORT_HELP);
		return 0;
	}

	amclib amc;
	int numCards = amc.amcEnumFirmwares();
	if (numCards <= 0) {
		ERR("No AMC devices found or enumeration failed.\n");
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	int ret = amc.amcInitialize();
	if (ret != 0) {
		ERR("Failed to initialize AMC devices (error code: %d)\n", ret);
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	// Dispatch enabled functional options, passing initialized AMC instance
	ze_result_t firstError = ZE_RESULT_SUCCESS;
	for (const auto &entry : amcCmds) {
		if (entry.second.enabled && entry.second.func != nullptr) {
			ze_result_t r = (this->*entry.second.func)(&amc, numCards);
			if (r != ZE_RESULT_SUCCESS && firstError == ZE_RESULT_SUCCESS) {
				firstError = r;
			}
		}
	}

	return firstError;
}

/**
 * @brief Performs GPU reset operation via AMC
 *
 * This function executes GPU reset operations through the AMC
 * It supports resetting individual devices or all devices, with
 * optional confirmation prompts. The operation uses parallel execution via
 * threads for improved performance when resetting multiple devices.
 *
 * @param[in] amc Pointer to initialized AMC library instance
 * @param[in] numCards Number of available AMC devices
 * @return ze_result_t ZE_RESULT_SUCCESS on success, error code on failure
 */
ze_result_t cmdAmc::gpuReset(amclib *amc, int numCards)
{
	int deviceId = -1;
	if (amcCmds[AMC_DEVICE].enabled) {
		try {
			deviceId = std::stoi(amcCmds[AMC_DEVICE].val);
		} catch (const std::exception &) {
			ERR("Invalid device ID: %s\n", amcCmds[AMC_DEVICE].val.c_str());
			ERR("Run with --help for more information.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	bool skipConfirmation = amcCmds[AMC_YES].enabled;

	if (!skipConfirmation) {
		if (deviceId < 0) {
			PRINT("\nWARNING: This operation will reset ALL GPU devices via AMC\n");
		} else {
			PRINT("\nWARNING: This operation will reset the GPU device %d via AMC\n", deviceId);
		}
		PRINT("Do you want to continue? (yes/no): ");

		std::string response;
		std::getline(std::cin, response);

		if (response != "yes" && response != "y" && response != "YES" && response != "Y") {
			PRINT("Operation cancelled!\n");
			return ZE_RESULT_SUCCESS;
		}
	}

	// Validate device ID against available cards
	if (deviceId >= 0 && deviceId >= numCards) {
		if (numCards == 1) {
			ERR("Invalid device ID %d. Only device 0 is available\n", deviceId);
		} else {
			ERR("Invalid device ID %d. Valid range: 0-%d\n", deviceId, numCards - 1);
		}
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	std::vector<std::thread> workers;
	std::atomic<int> failureCount{0};
	std::atomic<int> successCount{0};

	if (deviceId < 0) {
		DBG("Executing GPU reset via AMC for all %d devices...\n", numCards);
		for (int i = 0; i < numCards; i++) {
			workers.emplace_back([amc, i, &failureCount, &successCount]() {
				DBG("Resetting GPU device %d via AMC...\n", i);
				if (amc->amcGpuReset(i) != 0) {
					ERR("Failed to reset GPU device %d via AMC\n", i);
					failureCount.fetch_add(1, std::memory_order_relaxed);
				} else {
					DBG("Successfully reset GPU device %d via AMC\n", i);
					successCount.fetch_add(1, std::memory_order_relaxed);
				}
			});
		}
	} else {
		DBG("Executing GPU reset via AMC for device %d...\n", deviceId);
		workers.emplace_back([amc, deviceId, &failureCount, &successCount]() {
			if (amc->amcGpuReset(deviceId) != 0) {
				ERR("Failed to reset GPU device %d via AMC\n", deviceId);
				failureCount.fetch_add(1, std::memory_order_relaxed);
			} else {
				successCount.fetch_add(1, std::memory_order_relaxed);
			}
		});
	}

	for (auto &worker : workers) {
		if (worker.joinable()) {
			worker.join();
		}
	}

	if (failureCount.load() > 0) {
		ERR("Failed to reset %d GPU device(s) via AMC\n", failureCount.load());
		if (successCount.load() > 0) {
			PRINT("Successfully reset %d GPU device(s) via AMC\n", successCount.load());
		}
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	if (deviceId < 0) {
		PRINT("\nGPU reset successfully completed on all devices via AMC\n");
	} else {
		PRINT("\nGPU reset successfully completed on device %d via AMC\n", deviceId);
	}

	return ZE_RESULT_SUCCESS;
}
