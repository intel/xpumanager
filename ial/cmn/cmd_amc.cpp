/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_amc.h"
#include "debug.h"
#include <CLI/CLI.hpp>
#include "table_builder.h"
#include "amclib.h"
#include "printer.h"
#include <atomic>
#include <thread>
#include <vector>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <ctime>
#include <array>

static std::unordered_map<amcSubCmdType, amcSubCmdStruct> amcCmds = {
	{AMC_HELP, {}},
	{AMC_GPURESET, {.func = &cmdAmc::gpuReset}},
	{AMC_SENSOR, {.func = &cmdAmc::readSensor}},
	{AMC_SENSORID, {}},
	{AMC_FILE, {.func = &cmdAmc::readFile}},
	{AMC_FILE_TYPE, {}},
	{AMC_OP_FILENAME, {}},
	{AMC_DEVICE, {}},
	{AMC_YES, {}},
	{AMC_JSON, {}},
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
	TableBuilder table;
	table.addColumn("Property", 20, Align::Left).addColumn("Value", 40, Align::Left);

	if (jsonObj->contains("device")) {
		table.addRow("Device", (*jsonObj)["device"].get<std::string>());
	}
	if (jsonObj->contains("sensor_id")) {
		table.addRow("Sensor ID", std::to_string((*jsonObj)["sensor_id"].get<int>()));
	}
	for (auto &item : jsonObj->items()) {
		if (item.key() == "device" || item.key() == "sensor_id") {
			continue;
		}
		std::string value;
		if (item.value().is_string()) {
			value = item.value().get<std::string>();
		} else if (item.value().is_number_integer() && item.value().get<int64_t>() == -1) {
			value = "N/A";
		} else {
			value = item.value().dump();
		}
		table.addRow(item.key(), value);
	}

	PRINT("{}", table.toString().c_str());
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
	helpList.push_back(helpCmd(HEADING, "%s amc --gpuReset", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amc --gpuReset -y", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amc --gpuReset -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amc --gpuReset -d [deviceId] -y", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amc --sensor -d [deviceId] -s [sensorId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amc --sensor -d [deviceId] -s [sensorId] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amc --file -d [deviceId] --fileType [fileType] --fileName [outputFile]",
							   progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "--device,--id               Specify the device ID or PCI BDF address"));
	helpList.push_back(helpCmd(HEADING, "--gpuReset                  Reset GPU(s) via AMC"));
	helpList.push_back(helpCmd(HEADING, "--sensor                    Read AMC real-time sensor readings"));
	helpList.push_back(
		helpCmd(HEADING, "-s,--sensorId               Specify the sensor ID (Sensor IDs are listed below)"));
	helpList.push_back(helpCmd(HEADING, "--file                      Read a file from GPU via AMC"));
	helpList.push_back(helpCmd(
		HEADING, "--fileType                  Specify the type of file to read (Filetype ids are listed below)"));
	helpList.push_back(helpCmd(
		HEADING,
		"--fileName                  Specify the output file name. Default is <filepdrname_YrMthDt_HrMinSec>.bin"));
	helpList.push_back(helpCmd(HEADING, "-y,--yes                    Skip confirmation prompt"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Sensor IDs (used with -s/--sensorId):"));
	helpList.push_back(helpCmd(SUB_HEADING, "1. Temperature Sensor 0 from Add-In-Card"));
	helpList.push_back(helpCmd(SUB_HEADING, "2. Temperature Sensor 1 from Add-In-Card"));
	helpList.push_back(helpCmd(SUB_HEADING, "3. VR Voltage from Add-In-Card"));
	helpList.push_back(helpCmd(SUB_HEADING, "4. VR Current from Add-In-Card"));
	helpList.push_back(helpCmd(SUB_HEADING, "5. VR Power from Add-In-Card"));
	helpList.push_back(helpCmd(SUB_HEADING, "6. VR Temperature Sensor"));
	helpList.push_back(helpCmd(SUB_HEADING, "7. Total Board Power"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "File Types (used with --fileType):"));
	helpList.push_back(helpCmd(SUB_HEADING, "1. AMC crash logs"));
	helpList.push_back(helpCmd(SUB_HEADING, "2. System trace logs"));
	helpList.push_back(helpCmd(SUB_HEADING, "3. GPU crash logs"));
	helpList.push_back(helpCmd(SUB_HEADING, "4. GPU bulk telemetry logs"));
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

	// Reset state
	for (auto &[k, v] : amcCmds) {
		v.enabled = false;
		v.val.clear();
	}

	if (args->argc == 2) {
		help();
		return ZE_RESULT_SUCCESS;
	}

	CLI::App sub{"AMC firmware and sensor operations", "amc"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", amcCmds[AMC_JSON].enabled, "Print result in JSON format");
	sub.add_flag("--gpureset", amcCmds[AMC_GPURESET].enabled, "Reset a GPU");
	sub.add_flag("--sensor", amcCmds[AMC_SENSOR].enabled, "Read AMC sensor data");
	sub.add_option("-s,--sensorid", amcCmds[AMC_SENSORID].val, "Sensor ID")->each([&](const std::string &) {
		amcCmds[AMC_SENSORID].enabled = true;
	});
	sub.add_flag("--file", amcCmds[AMC_FILE].enabled, "Read a file from AMC");
	sub.add_option("--filetype", amcCmds[AMC_FILE_TYPE].val, "File type ID")->each([&](const std::string &) {
		amcCmds[AMC_FILE_TYPE].enabled = true;
	});
	sub.add_option("--filename", amcCmds[AMC_OP_FILENAME].val, "Output filename")->each([&](const std::string &) {
		amcCmds[AMC_OP_FILENAME].enabled = true;
	});
	sub.add_option("-d,--device,--id", amcCmds[AMC_DEVICE].val, "Device ID or PCI BDF address")
		->each([&](const std::string &) { amcCmds[AMC_DEVICE].enabled = true; });
	sub.add_flag("-y,--yes", amcCmds[AMC_YES].enabled, "Assume yes to all questions");

	try {
		sub.parse(args->argc - 1, args->argv + 1);
	} catch (const CLI::CallForHelp &) {
		help();
		return ZE_RESULT_SUCCESS;
	} catch (const CLI::ParseError &e) {
		ERR("{}", e.what());
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
		return ZE_RESULT_SUCCESS;
	}

	amclib amc;
	int numCards = amc.amcEnumFirmwares();
	if (numCards <= 0) {
		ERR("No AMC devices found or enumeration failed.\n");
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	int ret = amc.amcInitialize();
	if (ret != AMC_SUCCESS) {
		ERR("Failed to initialize AMC devices (error code: {})\n", ret);
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
 * @brief Get the index of an AMC card associated with the specified GPU BDF
 *
 * Searches the discovered AMC devices for a card associated with the given
 * GPU BDF (Bus:Device.Function) string and returns its index.
 *
 * @param gpuBDF GPU BDF string to identify the AMC card
 *
 * @return Card index if found, or -1 if not found
 * @retval >=0 Index of the found AMC card
 * @retval -1 No matching card found
 *
 * @note The function searches through the discovered AMC devices
 *       and retrieves information for the first matching GPU BDF.
 * @note The function returns -1 if no devices have been enumerated.
 */
ze_result_t cmdAmc::getDeviceIndex(amclib *amc, const std::string &val, int numCards, int &deviceIndex)
{
	deviceIndex = -1;
	if (val.empty())
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;

	try {
		size_t pos = 0;
		int parsed = std::stoi(val, &pos);
		if (pos == val.size()) {
			if (parsed < 0 || parsed >= numCards) {
				ERR("Device index {} is out of range. Valid range is 0 to {}.\n", parsed, numCards - 1);
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}
			deviceIndex = parsed;
			return ZE_RESULT_SUCCESS;
		}
	} catch (const std::exception &) {
		// Not a pure integer, treat as PCI BDF
	}

	if (amc == nullptr)
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;

	deviceIndex = amc->amcGetIndex(val);
	if (deviceIndex < 0 || deviceIndex >= numCards) {
		ERR("No AMC card found for GPU BDF: {}\n", val.c_str());
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	return ZE_RESULT_SUCCESS;
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
			ERR("Invalid device ID: {}\n", amcCmds[AMC_DEVICE].val.c_str());
			ERR("Run with --help for more information.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	bool skipConfirmation = amcCmds[AMC_YES].enabled;

	if (!skipConfirmation) {
		if (deviceId < 0) {
			PRINT("\nWARNING: This operation will reset ALL GPU devices via AMC\n");
		} else {
			PRINT("\nWARNING: This operation will reset the GPU device {} via AMC\n", deviceId);
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
			ERR("Invalid device ID {}. Only device 0 is available\n", deviceId);
		} else {
			ERR("Invalid device ID {}. Valid range: 0-{}\n", deviceId, numCards - 1);
		}
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	std::vector<std::thread> workers;
	std::atomic<int> failureCount{0};
	std::atomic<int> successCount{0};

	if (deviceId < 0) {
		DBG("Executing GPU reset via AMC for all {} devices...\n", numCards);
		for (int i = 0; i < numCards; i++) {
			workers.emplace_back([amc, i, &failureCount, &successCount]() {
				DBG("Resetting GPU device {} via AMC...\n", i);
				if (amc->amcGpuReset(i) != AMC_SUCCESS) {
					ERR("Failed to reset GPU device {} via AMC\n", i);
					failureCount.fetch_add(1, std::memory_order_relaxed);
				} else {
					DBG("Successfully reset GPU device {} via AMC\n", i);
					successCount.fetch_add(1, std::memory_order_relaxed);
				}
			});
		}
	} else {
		DBG("Executing GPU reset via AMC for device {}...\n", deviceId);
		workers.emplace_back([amc, deviceId, &failureCount, &successCount]() {
			if (amc->amcGpuReset(deviceId) != AMC_SUCCESS) {
				ERR("Failed to reset GPU device {} via AMC\n", deviceId);
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
		ERR("Failed to reset {} GPU device(s) via AMC\n", failureCount.load());
		if (successCount.load() > 0) {
			PRINT("Successfully reset {} GPU device(s) via AMC\n", successCount.load());
		}
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	if (deviceId < 0) {
		PRINT("\nGPU reset successfully completed on all devices via AMC\n");
	} else {
		PRINT("\nGPU reset successfully completed on device {} via AMC\n", deviceId);
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
	if (getDeviceIndex(amc, amcCmds[AMC_DEVICE].val, numCards, deviceIndex) != ZE_RESULT_SUCCESS) {
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	nlohmann::ordered_json outputJson;
	if (amcCmds[AMC_SENSORID].enabled) {
		if (!std::all_of(amcCmds[AMC_SENSORID].val.begin(), amcCmds[AMC_SENSORID].val.end(), ::isdigit)) {
			ERR("Invalid sensor ID: {}. Sensor ID must be a numeric value.\n", amcCmds[AMC_SENSORID].val.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		int sensorId;
		try {
			sensorId = std::stoi(amcCmds[AMC_SENSORID].val);
			if (sensorId > UINT16_MAX) {
				throw std::out_of_range("Sensor ID exceeds 16-bit limit");
			}
		} catch (const std::exception &) {
			ERR("Sensor ID '{}' is out of range (0-65535).\n", amcCmds[AMC_SENSORID].val.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}

		std::vector<amcSensorInfo> sensorInfo;
		int ret = amc->amcGetSensorInfoBySensorId(deviceIndex, static_cast<uint16_t>(sensorId), sensorInfo);
		if (ret != AMC_SUCCESS) {
			ERR("Failed to get AMC sensor info for sensor ID {} on device {}.\n", sensorId,
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

/**
 * @brief Executes the AMC file reading command
 *
 * This function implements the main execution logic for the AMC file command,
 * parsing command line arguments and retrieving specified files from the GPU via
 * the Advanced Management Controller. It validates input parameters, handles file
 * retrieval, and supports saving output to a specified file with error handling.
 *
 * @param[in] amc Pointer to initialized AMC library instance
 * @param[in] numCards Number of available AMC devices
 * @return ze_result_t ZE_RESULT_SUCCESS on success, error code on failure
 */
ze_result_t cmdAmc::readFile(amclib *amc, int numCards)
{
	TRACING();

	constexpr std::array<const char *, 5> filePdrNames = {
		"Unknown", "amc_crash_dump", "syst_log", "gpu_crash_log", "gpu_bulk_telemetry",
	};

	if (!amcCmds[AMC_DEVICE].enabled || amcCmds[AMC_DEVICE].val.empty()) {
		ERR("Device ID must be specified with --device option.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (!amcCmds[AMC_FILE_TYPE].enabled || amcCmds[AMC_FILE_TYPE].val.empty()) {
		ERR("File type must be specified with --filetype option.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	uint16_t filePdrId = 0;
	try {
		unsigned long parsed = std::stoul(amcCmds[AMC_FILE_TYPE].val);
		if (parsed > UINT16_MAX) {
			throw std::out_of_range("File type ID exceeds 16-bit limit");
		}
		filePdrId = static_cast<uint16_t>(parsed);
		if (filePdrId == 0 || filePdrId >= filePdrNames.size()) {
			ERR("Invalid file type ID: {}. Valid range is 1 to {}.\n", filePdrId, filePdrNames.size() - 1);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	} catch (const std::exception &) {
		ERR("Invalid file type: {}. File type must be a numeric value.\n", amcCmds[AMC_FILE_TYPE].val.c_str());
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	std::string opFilename{};
	if (amcCmds[AMC_OP_FILENAME].enabled && !amcCmds[AMC_OP_FILENAME].val.empty()) {
		opFilename = amcCmds[AMC_OP_FILENAME].val;
	} else {
		std::time_t now = std::time(nullptr);
		char timeStr[20];
		std::strftime(timeStr, sizeof(timeStr), "_%Y%m%d_%H%M%S", std::localtime(&now));
		opFilename = filePdrNames[filePdrId] + std::string(timeStr) + ".bin";
	}

	int deviceIndex = -1;
	if (getDeviceIndex(amc, amcCmds[AMC_DEVICE].val, numCards, deviceIndex) != ZE_RESULT_SUCCESS) {
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	std::vector<uint8_t> fileData;
	int ret = amc->amcReadFile(deviceIndex, filePdrId, fileData);
	if (ret != AMC_SUCCESS) {
		ERR("Failed to read AMC file of type {} on device {}.\n", filePdrId, amcCmds[AMC_DEVICE].val.c_str());
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	if (!fileData.empty()) {
		std::ofstream outFile(opFilename, std::ios::binary);
		if (!outFile) {
			ERR("Failed to open output file: {}\n", opFilename.c_str());
			return ZE_RESULT_ERROR_UNKNOWN;
		}
		outFile.write(reinterpret_cast<const char *>(fileData.data()), fileData.size());
		if (!outFile) {
			ERR("Failed to write data to output file: {}\n", opFilename.c_str());
			return ZE_RESULT_ERROR_UNKNOWN;
		}
		outFile.close();
		PRINT("Successfully read AMC file of type {} on device {} and saved to {}\n", filePdrId,
			  amcCmds[AMC_DEVICE].val.c_str(), opFilename.c_str());
	} else {
		ERR("AMC file of type {} on device {} is empty.\n", filePdrId, amcCmds[AMC_DEVICE].val.c_str());
		return ZE_RESULT_ERROR_UNKNOWN;
	}
	return ZE_RESULT_SUCCESS;
}