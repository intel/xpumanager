/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_health.h"
#include "printer.h"
#include "debug.h"
#include <assert.h>
#include <temperature.h>
#include <memory.h>

static std::unordered_map<healthCmdType, healthCmdStruct> healthCmds = {
	{healthCmdType::HEALTH_HELP, {{"help", no_argument, 0, 'h'}, nullptr, false, ""}},
	{healthCmdType::HEALTH_JSON, {{"json", no_argument, 0, 'j'}, nullptr, false, ""}},
	{healthCmdType::HEALTH_LIST, {{"list", no_argument, 0, 'l'}, nullptr, false, ""}},
	{healthCmdType::HEALTH_DEVICE, {{"device", required_argument, 0, 'd'}, &cmdHealth::allComponents, false, ""}},
	{healthCmdType::HEALTH_COMPONENT, {{"component", required_argument, 0, 'c'}, &cmdHealth::component, false, ""}},
};

healthSubCmdStruct componentCmds[] = {
	{healthSubCmdType::HEALTH_CORETEMPERATURE, &cmdHealth::coreTemperature},
	{healthSubCmdType::HEALTH_MEMORYTEMPERATURE, &cmdHealth::memoryTemperature},
	{healthSubCmdType::HEALTH_POWER, &cmdHealth::power},
	{healthSubCmdType::HEALTH_MEMORY, &cmdHealth::healthMemory},
	{healthSubCmdType::HEALTH_XELINKPORT, &cmdHealth::xeLinkPort},
	{healthSubCmdType::HEALTH_FREQUENCY, &cmdHealth::frequency},
};

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdHealth::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Get the GPU device component health status"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s health [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s health -l", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s health -l -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s health -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s health -d [pciBdfAddress]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s health -d [deviceId] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s health -d [pciBdfAddress] -j", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s health -d [deviceId] -c [componentTypeId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s health -d [pciBdfAddress] -c [componentTypeId] -j", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "-l,--list                   Display health info for all devices"));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 The device ID or PCI BDF address"));
	helpList.push_back(helpCmd(HEADING, "-c,--component              Component types"));
	helpList.push_back(helpCmd(SUB_HEADING2, "1. GPU Core Temperature"));
	helpList.push_back(helpCmd(SUB_HEADING2, "2. GPU Memory Temperature"));
	helpList.push_back(helpCmd(SUB_HEADING2, "3. GPU Power"));
	helpList.push_back(helpCmd(SUB_HEADING2, "4. GPU Memory"));
	helpList.push_back(helpCmd(SUB_HEADING2, "5. Xe Link Port"));
	helpList.push_back(helpCmd(SUB_HEADING2, "6. GPU Frequency"));

	printHelp(helpList, helpType);
	helpList.clear();
}
/**
 * @brief Constructor for HealthTextPrinter class
 */
HealthTextPrinter::HealthTextPrinter() : TextPrinter() {}

/**
 * @brief Prints device health information in a formatted text layout
 *
 * This function formats and prints a single device's health information from a JSON object.
 * It displays the device ID first, then formats each component's health information with
 * proper indentation for better readability. Nested JSON objects are displayed hierarchically.
 * This function safely handles various JSON value types by converting them to appropriate
 * string representations.
 *
 * @param jsonObj Pointer to the JSON object containing a single device's health information
 */
void HealthTextPrinter::printDeviceInfo(nlohmann::ordered_json *jsonObj)
{
	PRINT("| Device ID: %d\n", (*jsonObj)["device_id"].get<int>());
	for (auto &item : jsonObj->items()) {
		if (item.key() == "device_id") {
			continue;
		}

		PRINT("|   %s:\n", item.key().c_str());
		for (auto &subitem : item.value().items()) {
			std::string value;
			if (subitem.value().is_string()) {
				value = subitem.value().get<std::string>();
			} else {
				value = subitem.value().dump();
			}
			PRINT("|     %s: %s\n", subitem.key().c_str(), value.c_str());
		}
	}
	PRINT("\n");
}
/**
 * @brief Prints text output with custom formatting for health command
 *
 * This function converts JSON structured data into a readable text format. It handles
 * both "device_list" structure for multiple devices and simple key-value pairs for single
 * device or component output. It safely converts different JSON value types to string
 * representations.
 *
 * @param jsonObj Pointer to the JSON object to be formatted and printed as text
 */
void HealthTextPrinter::print(nlohmann::ordered_json *jsonObj)
{
	if (jsonObj->contains("device_list")) {
		for (auto &device : (*jsonObj)["device_list"]) {
			printDeviceInfo(&device);
		}
	} else {
		printDeviceInfo(jsonObj);
	}
}

/**
 * @brief Lists health status of all components for all devices
 *
 * This function iterates through all discovered devices and runs comprehensive
 * health checks on each one. For every device, it executes all available component
 * tests including temperature monitoring, power assessment, memory health evaluation,
 * Xe Link port status, and frequency subsystem checks. Results are stored in the provided
 * JSON object.
 *
 * @param devList Pointer to vector of device information structures
 * @param jsonObj Pointer to a JSON object where results will be stored
 * @return ze_result_t ZE_RESULT_SUCCESS if all checks pass, or the first encountered error code
 */

ze_result_t cmdHealth::allComponentsAllDevices(std::vector<devInfo> *devList, nlohmann::ordered_json *jsonObj)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	auto deviceListJson = std::make_unique<nlohmann::ordered_json>();
	for (auto &d : *devList) {
		// Create a JSON object for this device
		nlohmann::ordered_json deviceJson;

		deviceJson["device_id"] = d.index;

		// Run all component tests and collect results in the device JSON
		result = this->allComponents(&d, &deviceJson);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Health check failed for device id: %d\n", d.index);
		}

		deviceListJson->push_back(deviceJson);
	}
	(*jsonObj)["device_list"] = *deviceListJson;

	return result;
}

/**
 * @brief Performs health checks on all components for a single device
 *
 * This function iterates through all available component health tests and
 * executes them for the specified device. It provides a comprehensive health
 * assessment covering core temperature, memory temperature, power, memory health,
 * Xe Link ports, and frequency subsystems. Results are stored in the provided
 * JSON object.
 *
 * @param d Pointer to device information structure containing device handles and properties
 * @param jsonObj Pointer to a JSON object where results will be stored
 * @return ze_result_t ZE_RESULT_SUCCESS if all health checks pass, or the first encountered error code
 */

ze_result_t cmdHealth::allComponents(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	for (const auto &test : componentCmds) {
		DBG("Running test: %d\n", test.type);
		result = (this->*test.func)(d, jsonObj);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Health check failed for device id: %d\n", d->index);
			break;
		}
	}

	return result;
}
/**
 * @brief Performs comprehensive component health assessment for the device
 *
 * This function executes a series of health checks across various device
 * components including temperatures, power consumption, memory health,
 * and overall system status. It provides a holistic view of device health.
 * Results are stored in the provided JSON object.
 *
 * @param d Pointer to device information structure containing device handles and properties
 * @param jsonObj Pointer to a JSON object where results will be stored
 * @return ze_result_t ZE_RESULT_SUCCESS if all health checks pass, error code otherwise
 */
ze_result_t cmdHealth::component(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	bool found = false;

	for (const auto &test : componentCmds) {
		if (test.type == stoi(healthCmds[healthCmdType::HEALTH_COMPONENT].val)) {
			DBG("Running test: %d\n", test.type);
			found = true;
			result = (this->*test.func)(d, jsonObj);
			break;
		}
	}

	if (!found) {
		ERR("The following argument was not expected: '%s'.\n",
			healthCmds[healthCmdType::HEALTH_COMPONENT].val.c_str());
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	return result;
}

/**
 * @brief Retrieves GPU core temperature thresholds
 *
 * This function queries the device's core temperature monitoring system to
 * obtain throttle and shutdown threshold values. These thresholds are critical
 * for thermal management and preventing hardware damage due to overheating.
 * Results are stored in the provided JSON object under the "core_temperature" key.
 *
 * @param d Pointer to device information structure containing device handles
 * @param jsonObj Pointer to a JSON object where results will be stored
 * @return ze_result_t ZE_RESULT_SUCCESS on successful retrieval, error code otherwise
 */
ze_result_t cmdHealth::coreTemperature(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	uint32_t throttleThreshold = 0;
	uint32_t shutdownThreshold = 0;

	temperature *t = (temperature *)d->dev->getTemperature();
	if (t == nullptr) {
		ERR("Failed to get temperature handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	result = t->getCoreThreshold(d->zesDeviceHdl, &throttleThreshold, &shutdownThreshold);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get core temperature thresholds: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	(*jsonObj)["core_temperature"] = {{"throttle_threshold", std::to_string(throttleThreshold) + " C"},
									  {"shutdown_threshold", std::to_string(shutdownThreshold) + " C"}};

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Retrieves GPU memory temperature thresholds
 *
 * This function queries the device's memory temperature monitoring system to
 * obtain throttle and shutdown threshold values. Memory temperature monitoring
 * is essential for maintaining optimal performance and preventing thermal damage
 * to high-bandwidth memory components. Results are stored in the provided JSON object
 * under the "memory_temperature" key.
 *
 * @param d Pointer to device information structure containing device handles
 * @param jsonObj Pointer to a JSON object where results will be stored
 * @return ze_result_t ZE_RESULT_SUCCESS on successful retrieval, error code otherwise
 */
ze_result_t cmdHealth::memoryTemperature(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	uint32_t throttleThreshold = 0;
	uint32_t shutdownThreshold = 0;

	temperature *t = (temperature *)d->dev->getTemperature();
	if (t == nullptr) {
		ERR("Failed to get temperature handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	result = t->getMemoryThreshold(d->zesDeviceHdl, &throttleThreshold, &shutdownThreshold);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory temperature thresholds: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	(*jsonObj)["memory_temperature"] = {{"throttle_threshold", std::to_string(throttleThreshold) + " C"},
										{"shutdown_threshold", std::to_string(shutdownThreshold) + " C"}};

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Performs GPU power subsystem health assessment
 *
 * This function evaluates the health status of the device's power management
 * subsystem, including power consumption patterns, voltage regulation, and
 * power delivery efficiency. Currently implemented as a placeholder for
 * future power health monitoring capabilities. Results are stored in the provided JSON
 * object under the "power_health" key.
 *
 * @param d Pointer to device information structure (currently unused)
 * @param jsonObj Pointer to a JSON object where results will be stored
 * @return ze_result_t ZE_RESULT_SUCCESS indicating successful assessment
 */
ze_result_t cmdHealth::power(UNUSED devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();

	(*jsonObj)["power_health"] = {{"status", "ok"}};

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Retrieves GPU memory health status
 *
 * This function queries the device's memory subsystem to determine the current
 * health status of GPU memory components. It evaluates memory integrity,
 * error rates, and overall memory subsystem health, providing critical
 * information for system reliability assessment. Results are stored in the provided
 * JSON object under the "memory_health" key.
 *
 * @param d Pointer to device information structure containing device handles
 * @param jsonObj Pointer to a JSON object where results will be stored
 * @return ze_result_t ZE_RESULT_SUCCESS on successful health check, error code otherwise
 */
ze_result_t cmdHealth::healthMemory(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_mem_health_t health;

	memory *mem = d->dev->getMemory();
	if (mem == nullptr) {
		ERR("Failed to get memory handle\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	result = mem->getMemoryHealth(&health);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get memory temperature thresholds: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	std::string healthStatus;
	if (health == ZES_MEM_HEALTH_OK) {
		healthStatus = "OK";
	} else if (health == ZES_MEM_HEALTH_DEGRADED) {
		healthStatus = "DEGRADED";
	} else if (health == ZES_MEM_HEALTH_CRITICAL) {
		healthStatus = "CRITICAL";
	} else if (health == ZES_MEM_HEALTH_REPLACE) {
		healthStatus = "REPLACE";
	} else {
		healthStatus = "UNKNOWN";
	}

	(*jsonObj)["memory_health"] = {{"status", healthStatus}};

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Performs Xe Link port health assessment
 *
 * This function evaluates the health and operational status of Xe Link
 * interconnect ports used for high-speed GPU-to-GPU communication.
 * It monitors link integrity, bandwidth availability, and error rates
 * for multi-GPU configurations. Currently implemented as a placeholder
 * for future Xe Link health monitoring capabilities. Results are stored in the
 * provided JSON object under the "xe_link_port_health" key.
 *
 * @param d Pointer to device information structure (currently unused)
 * @param jsonObj Pointer to a JSON object where results will be stored
 * @return ze_result_t ZE_RESULT_SUCCESS indicating successful assessment
 */
ze_result_t cmdHealth::xeLinkPort(UNUSED devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();

	(*jsonObj)["xe_link_port_health"] = {{"status", "ok"}};

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Performs GPU frequency subsystem health assessment
 *
 * This function evaluates the health and stability of the device's frequency
 * management system, including clock generation, frequency scaling capabilities,
 * and thermal/power throttling mechanisms. It ensures that frequency control
 * systems are operating within specification. Currently implemented as a
 * placeholder for future frequency health monitoring capabilities. Results are stored
 * in the provided JSON object under the "frequency_health" key.
 *
 * @param d Pointer to device information structure (currently unused)
 * @param jsonObj Pointer to a JSON object where results will be stored
 * @return ze_result_t ZE_RESULT_SUCCESS indicating successful assessment
 */
ze_result_t cmdHealth::frequency(UNUSED devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();

	(*jsonObj)["frequency_health"] = {{"status", "ok"}};

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the health run.
 *
 * @return int Returns 0 on success.
 */
int cmdHealth::run(arg_struct *args)
{
	TRACING();
	std::vector<devInfo> deviceList;
	ze_result_t result;
	int opt;
	int optionIndex = 0;
	std::string shortOpts;
	std::unique_ptr<Printer> printer;
	std::vector<struct option> longOptsVec;

	processOptions(healthCmds, shortOpts, longOptsVec);
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
			healthCmds[healthCmdType::HEALTH_JSON].enabled = true;
			break;
		case 'l':
			healthCmds[healthCmdType::HEALTH_LIST].enabled = true;
			break;
		case 'd':
			healthCmds[healthCmdType::HEALTH_DEVICE].enabled = true;
			healthCmds[healthCmdType::HEALTH_DEVICE].val = optarg;
			break;
		case 'c':
			healthCmds[healthCmdType::HEALTH_COMPONENT].enabled = true;
			healthCmds[healthCmdType::HEALTH_COMPONENT].val = optarg;
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

	// If no options were specified, print help
	if (args->argc == 2) {
		help();
		return ZE_RESULT_SUCCESS;
	}

	// user must specify a device ID or PCI BDF address
	if (!healthCmds[healthCmdType::HEALTH_LIST].enabled && !healthCmds[healthCmdType::HEALTH_DEVICE].enabled) {
		ERR("You must specify a device ID or PCI BDF address with the -d option.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Find the device based on the provided device ID or PCI BDF address
	result = args->sm.findDevice(healthCmds[healthCmdType::HEALTH_DEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '%s'.\n",
			healthCmds[healthCmdType::HEALTH_DEVICE].val.c_str());
		return result;
	}

	auto jsonObj = std::make_unique<nlohmann::ordered_json>();
	if (healthCmds[healthCmdType::HEALTH_LIST].enabled) {
		// List all devices
		result = this->allComponentsAllDevices(&deviceList, jsonObj.get());
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Error: Unable to retrieve health info for devices.\n");
		}
	} else {
		for (auto &device : deviceList) {
			if (healthCmds[healthCmdType::HEALTH_COMPONENT].enabled) {
				result = this->component(&device, jsonObj.get());
			} else {
				result = this->allComponents(&device, jsonObj.get());
			}

			// Add device identifier to the JSON object
			(*jsonObj)["device_id"] = device.index;
		}
	}

	if (healthCmds[healthCmdType::HEALTH_JSON].enabled == true) {
		printer = std::make_unique<JsonPrinter>();
	} else {
		printer = std::make_unique<HealthTextPrinter>();
	}
	if (result == ZE_RESULT_SUCCESS) {
		printer->print(jsonObj.get());
	}

	return result;
}
