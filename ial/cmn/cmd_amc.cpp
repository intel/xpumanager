/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_amc.h"
#include "debug.h"
#include "amclib.h"
#include "printer.h"
#include <atomic>
#include <thread>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <nlohmann/json.hpp>

static std::unordered_map<amcSubCmdType, amcSubCmdStruct> amcCmds = {
	{AMC_HELP, {{"help", no_argument, 0, 'h'}, nullptr, false, ""}},
	{AMC_GPURESET, {{"gpureset", no_argument, 0, 0}, &cmdAmc::gpuReset, false, ""}},
	{AMC_SENSOR, {{"sensor", no_argument, 0, 0}, &cmdAmc::readSensor, false, ""}},
	{AMC_SENSORID, {{"sensorid", required_argument, 0, 's'}, nullptr, false, ""}},
	{AMC_DEVICE, {{"device", required_argument, 0, 'd'}, nullptr, false, ""}},
	{AMC_YES, {{"yes", no_argument, 0, 'y'}, nullptr, false, ""}},
	{AMC_JSON, {{"json", no_argument, 0, 'j'}, nullptr, false, ""}},
};

AmcTextPrinter::AmcTextPrinter() : TextPrinter() {}

/**
 * @brief Prints detailed AMC sensor device information
 *
 * This function formats and prints detailed information about the AMC sensor
 * device, including device identifier, sensor ID, and various sensor readings.
 *
 * @param[in] jsonObj Pointer to the JSON object containing AMC sensor data
 */
void AmcTextPrinter::printDeviceInfo(nlohmann::ordered_json *jsonObj)
{
	PRINT("| Device: %s\n", (*jsonObj)["device"].get<std::string>().c_str());
	if (jsonObj->contains("sensor_id")) {
		PRINT("| Sensor ID: %d\n", (*jsonObj)["sensor_id"].get<int>());
	}
	for (auto &item : jsonObj->items()) {
		if (item.key() == "device" || item.key() == "sensor_id") {
			continue;
		}
		std::string value;
		if (item.value().is_string()) {
			value = item.value().get<std::string>();
		} else {
			value = item.value().dump();
		}
		PRINT("| %s: %s\n", item.key().c_str(), value.c_str());
	}
	PRINT("\n");
}

/**
 * @brief Prints the AMC sensor information in text format
 *
 * This function overrides the base class print method to format and display
 * AMC sensor information in a human-readable text format.
 *
 * @param[in] jsonObj Pointer to the JSON object containing AMC sensor data
 */
void AmcTextPrinter::print(nlohmann::ordered_json *jsonObj) { printDeviceInfo(jsonObj); }
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
	helpList.push_back(helpCmd(HEADING, "%s amc --sensor -d [deviceId] -s [sensorId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amc --sensor -d [deviceId] -s [sensorId] -j", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "--gpureset                  Reset GPU(s) via AMC"));
	helpList.push_back(helpCmd(HEADING, "--sensor                    Read AMC real-time sensor readings"));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 Specify the device ID or PCI BDF address"));
	helpList.push_back(
		helpCmd(HEADING, "-s,--sensorid               Specify the sensor ID (Sensor IDs are listed below)"));
	helpList.push_back(helpCmd(HEADING, "-y,--yes                    Skip confirmation prompt"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Sensor IDs (used with -s/--sensorid):"));
	helpList.push_back(helpCmd(SUB_HEADING, "1. Temperature Sensor 0 from Add-In-Card"));
	helpList.push_back(helpCmd(SUB_HEADING, "2. Temperature Sensor 1 from Add-In-Card"));
	helpList.push_back(helpCmd(SUB_HEADING, "3. VR Voltage from Add-In-Card"));
	helpList.push_back(helpCmd(SUB_HEADING, "4. VR Current from Add-In-Card"));
	helpList.push_back(helpCmd(SUB_HEADING, "5. VR Power from Add-In-Card"));
	helpList.push_back(helpCmd(SUB_HEADING, "6. VR Temperature Sensor"));
	helpList.push_back(helpCmd(SUB_HEADING, "7. Total Board Power"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "WARNING:"));
	helpList.push_back(helpCmd(HEADING, "  - The operation may require root/administrator privileges"));

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
		case 'j':
			amcCmds[AMC_JSON].enabled = true;
			break;
		case 's':
			amcCmds[AMC_SENSORID].enabled = true;
			amcCmds[AMC_SENSORID].val = optarg;
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
	if (ret != AMC_SUCCESS) {
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
	TRACING();
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
				if (amc->amcGpuReset(i) != AMC_SUCCESS) {
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
			if (amc->amcGpuReset(deviceId) != AMC_SUCCESS) {
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

/**
 * @brief Executes the AMC sensor monitoring command
 *
 * This function implements the main execution logic for the AMC sensor command,
 * parsing command line arguments and retrieving real-time sensor readings from
 * the Advanced Management Controller. It supports both standard and JSON output
 * formats for sensor data presentation.
 *
 * @param[in] amc Pointer to initialized AMC library instance
 * @param[in] numCards Number of available AMC devices
 * @return ze_result_t ZE_RESULT_SUCCESS on success, error code on failure
 */
ze_result_t cmdAmc::readSensor(amclib *amc, int numCards)
{
	TRACING();
	if (!amcCmds[AMC_DEVICE].enabled || amcCmds[AMC_DEVICE].val.empty()) {
		ERR("Device ID must be specified with --device option.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (!amcCmds[AMC_SENSORID].enabled || amcCmds[AMC_SENSORID].val.empty()) {
		ERR("Sensor ID must be specified with --sensorid option.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	int deviceIndex = -1;
	if (std::all_of(amcCmds[AMC_DEVICE].val.begin(), amcCmds[AMC_DEVICE].val.end(), ::isdigit)) {
		try {
			deviceIndex = std::stoi(amcCmds[AMC_DEVICE].val);
		} catch (const std::exception &) {
			ERR("Device index '%s' is out of range.\n", amcCmds[AMC_DEVICE].val.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		if (deviceIndex < 0 || deviceIndex >= numCards) {
			ERR("Device index %d is out of range. Valid range is 0 to %d.\n", deviceIndex, numCards - 1);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	} else {
		deviceIndex = amc->amcGetIndex(amcCmds[AMC_DEVICE].val);
		if (deviceIndex < 0) {
			ERR("No AMC card found for GPU BDF: %s\n", amcCmds[AMC_DEVICE].val.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	nlohmann::ordered_json outputJson;
	if (amcCmds[AMC_SENSORID].enabled) {
		if (!std::all_of(amcCmds[AMC_SENSORID].val.begin(), amcCmds[AMC_SENSORID].val.end(), ::isdigit)) {
			ERR("Invalid sensor ID: %s. Sensor ID must be a numeric value.\n", amcCmds[AMC_SENSORID].val.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		int sensorId;
		try {
			sensorId = std::stoi(amcCmds[AMC_SENSORID].val);
			if (sensorId > UINT16_MAX) {
				throw std::out_of_range("Sensor ID exceeds 16-bit limit");
			}
		} catch (const std::exception &) {
			ERR("Sensor ID '%s' is out of range (0-65535).\n", amcCmds[AMC_SENSORID].val.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		std::vector<amcSensorInfo> sensorInfo;
		int ret = amc->amcGetSensorInfoBySensorId(deviceIndex, static_cast<uint16_t>(sensorId), sensorInfo);
		if (ret != AMC_SUCCESS) {
			ERR("Failed to get AMC sensor info for sensor ID %d on device %s.\n", sensorId,
				amcCmds[AMC_DEVICE].val.c_str());
			return ZE_RESULT_ERROR_UNINITIALIZED;
		}

		outputJson["device"] = amcCmds[AMC_DEVICE].val;
		outputJson["sensor_id"] = sensorInfo[0].sensorId;
		outputJson["sensor_value"] = sensorInfo[0].sensorReading;
	}

	std::unique_ptr<Printer> printer;
	if (amcCmds[AMC_JSON].enabled) {
		printer = std::make_unique<JsonPrinter>();
	} else {
		printer = std::make_unique<AmcTextPrinter>();
	}
	if (!outputJson.empty()) {
		printer->print(&outputJson);
	}

	return ZE_RESULT_SUCCESS;
}
