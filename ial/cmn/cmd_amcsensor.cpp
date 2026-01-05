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

#include "cmd_amcsensor.h"
#include "amclib.h"
#include "debug.h"
#include <nlohmann/json.hpp>
#include <algorithm>

static std::unordered_map<amcCmdType, amcCmdStruct> amcCmdMap = {
	{AMCHELP, {{"help", no_argument, 0, 'h'}, false, ""}},
	{AMCJSON, {{"json", no_argument, 0, 'j'}, false, ""}},
	{AMCINFO, {{"info", no_argument, 0, 'i'}, false, ""}},
	{AMCSENSOR, {{"sensor", required_argument, 0, 's'}, false, ""}},
	{AMCDEVICE, {{"device", required_argument, 0, 'd'}, false, ""}},

};

/**
 * @brief Displays help information for the AMC sensor command
 *
 * This function generates and displays comprehensive help information for the
 * AMC (Advanced Management Controller) sensor command, including usage examples,
 * available options, and command descriptions for real-time sensor monitoring.
 *
 * @param[in] helpType Type of help to display (SHORT_HELP for brief, FULL_HELP for detailed)
 */
void cmdAmcSensor::help(HELP helpType)
{
	std::vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "List the AMC real-time sensor readings."));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s amcsensor [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amcsensor", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s amcsensor -j", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(HEADING, "-i,--info                   Display AMC card information"));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 The device PCI BDF address or index"));
	helpList.push_back(helpCmd(HEADING, "-s,--sensor                 The sensor ID to read"));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. Temperature Sensor 0 from Add-In-Card", 1));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. Temperature Sensor 1 from Add-In-Card", 2));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. VR Voltage from Add-In-Card", 3));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. VR Current from Add-In-Card", 4));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. VR Power from Add-In-Card", 5));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. VR Temperature Sensor", 6));
	helpList.push_back(helpCmd(SUB_HEADING, "%d. Total Board Power", 7));
	helpList.push_back(helpCmd(BLANK));

	printHelp(helpList, helpType);
	helpList.clear();
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
 * @brief Executes the AMC sensor monitoring command
 *
 * This function implements the main execution logic for the AMC sensor command,
 * parsing command line arguments and retrieving real-time sensor readings from
 * the Advanced Management Controller. It supports both standard and JSON output
 * formats for sensor data presentation.
 *
 * @param[in] args Pointer to argument structure containing command line parameters
 * @return int Return code: 0 on success, error code on failure
 */
int cmdAmcSensor::run(arg_struct *args)
{
	TRACING();
	std::string shortOpts;
	std::vector<struct option> longOptsVec;

	processOptions(amcCmdMap, shortOpts, longOptsVec);
	const struct option *longOpts = longOptsVec.data();

	int opt;
	int optionIndex = 0;

	// Skip the first two arguments (process and command name)
	int startind = 2;
	optind = 2;

	// If the user didn't provide any arguments, show help
	if (args->argc == 2) {
		help();
		return ZE_RESULT_SUCCESS;
	}

	while ((opt = getopt_long(args->argc, args->argv, shortOpts.c_str(), longOpts, &optionIndex)) != -1) {
		switch (opt) {
		case 'h':
			help();
			return 0;
		case 'j':
			amcCmdMap[AMCJSON].enabled = true;
			break;
		case 'i':
			amcCmdMap[AMCINFO].enabled = true;
			break;
		case 'd':
			amcCmdMap[AMCDEVICE].enabled = true;
			amcCmdMap[AMCDEVICE].val = std::string(optarg);
			break;
		case 's':
			amcCmdMap[AMCSENSOR].enabled = true;
			amcCmdMap[AMCSENSOR].val = std::string(optarg);
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

	if (amcCmdMap[AMCDEVICE].val.empty()) {
		ERR("Device BDF must be specified with --device option.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (!amcCmdMap[AMCSENSOR].enabled) {
		ERR("Sensor ID must be specified with --sensor option.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	amclib amcClient;

	int numCards = amcClient.amcEnumFirmwares();
	if (numCards <= 0) {
		ERR("No AMC devices found.\n");
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}
	int deviceIndex = -1;
	if (std::all_of(amcCmdMap[AMCDEVICE].val.begin(), amcCmdMap[AMCDEVICE].val.end(), ::isdigit)) {
		try {
			deviceIndex = std::stoi(amcCmdMap[AMCDEVICE].val);
		} catch (const std::exception &) {
			ERR("Device index '%s' is out of range.\n", amcCmdMap[AMCDEVICE].val.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		if (deviceIndex < 0 || deviceIndex >= numCards) {
			ERR("Device index %d is out of range. Valid range is 0 to %d.\n", deviceIndex, numCards - 1);
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	} else {
		deviceIndex = amcClient.amcGetIndex(amcCmdMap[AMCDEVICE].val);
		if (deviceIndex < 0) {
			ERR("No AMC card found for GPU BDF: %s\n", amcCmdMap[AMCDEVICE].val.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}
	if (amcClient.amcInitialize() != 0) {
		ERR("Failed to initialize AMC library.\n");
		return ZE_RESULT_ERROR_UNINITIALIZED;
	}

	nlohmann::ordered_json outputJson;
	if (amcCmdMap[AMCSENSOR].enabled) {
		if (!std::all_of(amcCmdMap[AMCSENSOR].val.begin(), amcCmdMap[AMCSENSOR].val.end(), ::isdigit)) {
			ERR("Invalid sensor ID: %s. Sensor ID must be a numeric value.\n", amcCmdMap[AMCSENSOR].val.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		int sensorId;
		try {
			sensorId = std::stoi(amcCmdMap[AMCSENSOR].val);
			if (sensorId > UINT16_MAX) {
				throw std::out_of_range("Sensor ID exceeds 16-bit limit");
			}
		} catch (const std::exception &) {
			ERR("Sensor ID '%s' is out of range (0-65535).\n", amcCmdMap[AMCSENSOR].val.c_str());
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
		std::vector<amcSensorInfo> sensorInfo;
		int ret = amcClient.amcGetSensorInfoBySensorId(deviceIndex, static_cast<uint16_t>(sensorId), sensorInfo);
		if (ret != 0) {
			ERR("Failed to get AMC sensor info for sensor ID %d on device %s.\n", sensorId,
				amcCmdMap[AMCDEVICE].val.c_str());
			return ZE_RESULT_ERROR_UNINITIALIZED;
		}
		outputJson["device"] = amcCmdMap[AMCDEVICE].val;
		outputJson["sensor_id"] = sensorInfo[0].sensorId;
		outputJson["sensor_value"] = sensorInfo[0].sensorReading;
	}

	std::unique_ptr<Printer> printer;
	if (amcCmdMap[AMCJSON].enabled) {
		printer = std::make_unique<JsonPrinter>();
	} else {
		printer = std::make_unique<AmcTextPrinter>();
	}
	if (!outputJson.empty()) {
		printer->print(&outputJson);
	}
	return 0;
}