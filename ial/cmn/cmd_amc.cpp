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
#include "utility/watchdog/watchdog.h"

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

static const char *sensorUnitToString(sensorUnits unit)
{
	switch (unit) {
	case PLDM_UNIT_DEGREES_C:
		return "degrees C";
	case PLDM_UNIT_VOLTS:
		return "V";
	case PLDM_UNIT_CURRENT_AMPS:
		return "A";
	case PLDM_UNIT_WATTS:
		return "W";
	case PLDM_UNIT_JOULES:
		return "J";
	case PLDM_UNIT_COULOMBS:
		return "C";
	default:
		return "unspecified";
	}
}

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
	helpList.push_back(helpCmd(HEADING, "%s amc --gpureset", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amc --gpureset -y", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amc --gpureset -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amc --gpureset -d [deviceId] -y", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amc --sensor -d [deviceId] -s [sensorId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amc --sensor -d [deviceId] -s [sensorId] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amc --file -d [deviceId] --filetype [filetype] --filename [outputFile]",
							   progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "--device,--id               Specify the device ID or PCI BDF address"));
	helpList.push_back(helpCmd(HEADING, "--gpureset                  Reset GPU(s) via AMC"));
	helpList.push_back(helpCmd(HEADING, "--sensor                    Read AMC real-time sensor readings"));
	helpList.push_back(
		helpCmd(HEADING, "-s,--sensorId               Specify the sensor ID (Sensor IDs are listed below)"));
	helpList.push_back(helpCmd(HEADING, "--file                      Read a file from GPU via AMC"));
	helpList.push_back(helpCmd(
		HEADING, "--filetype                  Specify the type of file to read (Filetype ids are listed below)"));
	helpList.push_back(helpCmd(
		HEADING,
		"--filename                  Specify the output file name. Default is <filepdrname_YrMthDt_HrMinSec>.bin"));
	helpList.push_back(helpCmd(HEADING, "-y,--yes                    Skip confirmation prompt"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Sensor IDs (used with -s/--sensorId):"));
	helpList.push_back(helpCmd(SUB_HEADING, "1. Temperature sensor_0 from Add-In-Card"));
	helpList.push_back(helpCmd(SUB_HEADING, "2. Temperature sensor_1 from Add-In-Card"));
	helpList.push_back(helpCmd(SUB_HEADING, "3. VR VCCGT temperature_0 from Add-In-Card"));
	helpList.push_back(helpCmd(SUB_HEADING, "4. VR VCCGT input power_0 from Add-In-Card"));
	helpList.push_back(helpCmd(SUB_HEADING, "5. VR VCCGT input voltage_0 from Add-In-Card"));
	helpList.push_back(helpCmd(SUB_HEADING, "6. VR VCCGT input current_0 from Add-In-Card"));
	helpList.push_back(helpCmd(SUB_HEADING, "85. Card average power"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "File Types (used with --filetype):"));
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
 * @brief AMC operations go over MCTP, so no Level Zero driver is needed.
 *
 * @param[in] cmdArgs  argv tokens after the subcommand name (unused).
 *
 * @retval DriverMode::None  Always.
 */
DriverMode cmdAmc::driverMode(UNUSED std::span<char *const> cmdArgs) const { return DriverMode::None; }

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
	sub.add_option("-d,--device,--id", amcCmds[AMC_DEVICE].val,
				   "Device ID, as other commands number devices, or PCI BDF address (single device only)")
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
 * @brief Resolve a --device value to the index of its AMC card
 *
 * A device index means the same GPU as in every other xpu-smi command. amc runs
 * without Level Zero, so the index is looked up in the device order predicted from
 * the OS (getLevelZeroGpuOrder()), and the AMC card is then found by that GPU's BDF.
 * Where the OS layer cannot predict the order, the index counts AMC cards in
 * discovery order instead. Any other value is taken as a GPU BDF.
 *
 * @param[in]  amc         Initialized AMC library instance
 * @param[in]  val         Device index or GPU BDF, as given on the command line
 * @param[in]  numCards    Number of discovered AMC cards
 * @param[out] deviceIndex Index of the AMC card, or -1 on failure
 *
 * @retval ZE_RESULT_SUCCESS                @p deviceIndex names the AMC card of that GPU
 * @retval ZE_RESULT_ERROR_INVALID_ARGUMENT No such device, or the device has no AMC card
 */
ze_result_t cmdAmc::getDeviceIndex(amclib *amc, const std::string &val, int numCards, int &deviceIndex)
{
	deviceIndex = -1;
	if (val.empty() || amc == nullptr)
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;

	std::string bdf = val;
	try {
		size_t pos = 0;
		int parsed = std::stoi(val, &pos);
		if (pos == val.size()) {
			std::vector<std::string> gpus;
			if (getLevelZeroGpuOrder(&gpus) < 0) {
				if (parsed < 0 || parsed >= numCards) {
					ERR("Device index {} is out of range. Valid range is 0 to {}.\n", parsed, numCards - 1);
					return ZE_RESULT_ERROR_INVALID_ARGUMENT;
				}
				deviceIndex = parsed;
				DBG("Device {} is AMC card {} ({}) in discovery order, GPU {}\n", val.c_str(), deviceIndex,
					amc->amcGetDevicePath(static_cast<uint32_t>(deviceIndex)).c_str(),
					amc->amcGetGpuBdf(static_cast<uint32_t>(deviceIndex)).c_str());
				return ZE_RESULT_SUCCESS;
			}
			if (gpus.empty()) {
				ERR("No GPU devices found for device index {}. Specify the device by PCI BDF address.\n", parsed);
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}
			if (parsed < 0 || static_cast<size_t>(parsed) >= gpus.size()) {
				ERR("Device index {} is out of range. Valid range is 0 to {}.\n", parsed, gpus.size() - 1);
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}
			bdf = gpus[parsed];
		}
	} catch (const std::exception &) {
		// Not a pure integer, treat as PCI BDF
	}

	deviceIndex = amc->amcGetIndex(bdf);
	if (deviceIndex < 0 || deviceIndex >= numCards) {
		if (bdf != val) {
			ERR("Device {} ({}) has no AMC card.\n", val.c_str(), bdf.c_str());
		} else {
			ERR("No AMC card found for GPU BDF: {}\n", val.c_str());
		}
		deviceIndex = -1;
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	DBG("Device {} is GPU {}, AMC card {} ({})\n", val.c_str(), bdf.c_str(), deviceIndex,
		amc->amcGetDevicePath(static_cast<uint32_t>(deviceIndex)).c_str());
	return ZE_RESULT_SUCCESS;
}

/// Prompt the user to confirm a reset of the GPU at BDF @p device (empty for all). Returns true if confirmed.
static bool confirmReset(const std::string &device)
{
	// Waiting for the user is not a hang.
	const auto prompt = xpum::watchdog::suspend("confirmation prompt");
	if (device.empty()) {
		PRINT("\nWARNING: This operation will reset ALL GPU devices via AMC\n");
	} else {
		PRINT("\nWARNING: This operation will reset GPU {} via AMC\n", device.c_str());
	}
	PRINT("Do you want to continue? (yes/no): ");
	std::string response;
	std::getline(std::cin, response);
	return response == "yes" || response == "y" || response == "YES" || response == "Y";
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
		ze_result_t r = getDeviceIndex(amc, amcCmds[AMC_DEVICE].val, numCards, deviceId);
		if (r != ZE_RESULT_SUCCESS) {
			ERR("Run with --help for more information.\n");
			return r;
		}
	}

	// Name the GPU by BDF; empty means all. Fall back to the -d value if the BDF is unknown.
	std::string target;
	if (deviceId >= 0) {
		target = amc->amcGetGpuBdf(static_cast<uint32_t>(deviceId));
		if (target.empty()) {
			target = amcCmds[AMC_DEVICE].val;
		}
	}
	if (!amcCmds[AMC_YES].enabled && !confirmReset(target)) {
		PRINT("Operation cancelled!\n");
		return ZE_RESULT_SUCCESS;
	}

	// Build the list of device indices to reset (-1 means all)
	std::vector<int> targets;
	if (deviceId < 0) {
		for (int i = 0; i < numCards; i++) {
			targets.push_back(i);
		}
	} else {
		targets.push_back(deviceId);
	}

	// Each reset masks Surprise Link Down, removes the GPU and rescans it; exiting part way
	// leaves the GPU off the bus or Surprise Link Down masked. Held until every worker joins.
	const auto resetting = xpum::watchdog::suspend("GPU reset via AMC");
	std::atomic<int> failureCount{0};
	std::atomic<int> successCount{0};
	std::vector<std::thread> workers;
	workers.reserve(targets.size());

	for (int id : targets) {
		DBG("Executing GPU reset via AMC for device {}...\n", id);
		workers.emplace_back([amc, id, &failureCount, &successCount]() {
			// Name the GPU by BDF: id indexes AMC cards, not xpu-smi devices.
			const std::string gpu = amc->amcGetGpuBdf(static_cast<uint32_t>(id));
			if (amc->amcGpuReset(id) != AMC_SUCCESS) {
				ERR("Failed to reset GPU {} via AMC\n", gpu.c_str());
				failureCount.fetch_add(1, std::memory_order_relaxed);
			} else {
				DBG("Successfully reset GPU {} via AMC\n", gpu.c_str());
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

	PRINT("\nGPU reset successfully completed on {} device(s) via AMC\n", static_cast<int>(targets.size()));
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
			ERR("Failed to get AMC sensor info for sensor ID {} on GPU {}.\n", sensorId,
				amc->amcGetGpuBdf(static_cast<uint32_t>(deviceIndex)).c_str());
			return ZE_RESULT_ERROR_UNINITIALIZED;
		}

		outputJson["device"] = amcCmds[AMC_DEVICE].val;
		outputJson["sensor_id"] = sensorInfo[0].sensorId;
		outputJson["sensor_value"] = sensorInfo[0].sensorReading;
		outputJson["sensor_unit"] = sensorUnitToString(sensorInfo[0].sensorUnit);
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

	// Messages name the GPU by BDF: the same device whether it was selected by ID or BDF.
	const std::string gpu = amc->amcGetGpuBdf(static_cast<uint32_t>(deviceIndex));
	std::vector<uint8_t> fileData;
	int ret = amc->amcReadFile(deviceIndex, filePdrId, fileData);
	if (ret != AMC_SUCCESS) {
		ERR("Failed to read AMC file of type {} on GPU {}.\n", filePdrId, gpu.c_str());
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
		PRINT("Successfully read AMC file of type {} on GPU {} and saved to {}\n", filePdrId, gpu.c_str(),
			  opFilename.c_str());
	} else {
		ERR("AMC file of type {} on GPU {} is empty.\n", filePdrId, gpu.c_str());
		return ZE_RESULT_ERROR_UNKNOWN;
	}
	return ZE_RESULT_SUCCESS;
}