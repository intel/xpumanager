/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_health.h"
#include "printer.h"
#include "debug.h"
#include "table_builder.h"
#include <assert.h>
#include <algorithm>
#include <charconv>
#include <optional>
#include <CLI/CLI.hpp>
#include <temperature.h>
#include <memory>
#include <sstream>
#include <iomanip>
#include <thread>
#include <inttypes.h>
#include "utility/watchdog/watchdog.h"

namespace {
/**
 * @brief Sets a health failure status in the JSON object
 *
 * This helper function adds a standardized health failure entry to a JSON object
 * with "Unknown" status and the provided description message.
 *
 * @param [in,out] jsonObj Pointer to the JSON object where the failure will be stored
 * @param [in] jsonKey The key name to use in the JSON object for this health component
 * @param [in] description Human-readable description of the health failure
 */
void setHealthFailureJson(nlohmann::ordered_json *jsonObj, const std::string &jsonKey, const std::string &description)
{
	(*jsonObj)[jsonKey] = {{"status", "Unknown"}, {"description", description}};
}

/**
 * @brief Sets a temperature failure status in the JSON object
 *
 * This helper function adds a standardized temperature failure entry to a JSON object
 * with "Unknown" status, initialized thresholds, and the provided description message.
 *
 * @param [in,out] jsonObj Pointer to the JSON object where the failure will be stored
 * @param [in] jsonKey The key name to use in the JSON object for this temperature sensor
 * @param [in] description Human-readable description of the temperature failure
 */
void setTemperatureFailureJson(nlohmann::ordered_json *jsonObj, const std::string &jsonKey,
							   const std::string &description)
{
	(*jsonObj)[jsonKey] = {
		{"status", "Unknown"}, {"throttle_threshold", -1}, {"shutdown_threshold", -1}, {"description", description}};
}

// A bare `health` prints help without touching a device;
// run() and driverMode() both use this so they cannot disagree.
[[nodiscard]] bool printsHelpOnly(std::span<char *const> cmdArgs) { return cmdArgs.empty(); }
} // namespace

static std::unordered_map<healthCmdType, healthCmdStruct> healthCmds = {
	{healthCmdType::HEALTH_HELP, {}},
	{healthCmdType::HEALTH_JSON, {}},
	{healthCmdType::HEALTH_LIST, {}},
	{healthCmdType::HEALTH_DEVICE, {.func = &cmdHealth::allComponents}},
	{healthCmdType::HEALTH_COMPONENT, {.func = &cmdHealth::component}},
};

healthSubCmdStruct componentCmds[] = {
	{healthSubCmdType::HEALTH_CORETEMPERATURE, &cmdHealth::coreTemperature, false},
	{healthSubCmdType::HEALTH_MEMORYTEMPERATURE, &cmdHealth::memoryTemperature, false},
	{healthSubCmdType::HEALTH_POWER, &cmdHealth::gpuPower, false},
	{healthSubCmdType::HEALTH_FREQUENCY, &cmdHealth::frequency, true},
};

/**
 * @brief Parses a -c value into a component ID listed in componentCmds
 *
 * Accepts only a whole decimal number that names a known component, so "abc",
 * "4x" and unlisted IDs such as 5 and 6 are all rejected instead of throwing
 * (std::stoi) or matching a prefix.
 *
 * @param [in] value The text given to -c
 * @return The component ID, or std::nullopt if it is not a known component
 */
static std::optional<int> knownComponentId(const std::string &value)
{
	int id = 0;
	const char *first = value.data();
	const char *last = first + value.size();
	const auto [ptr, ec] = std::from_chars(first, last, id);
	if (ec != std::errc{} || ptr != last) {
		return std::nullopt;
	}
	const bool known = std::ranges::any_of(componentCmds, [id](const auto &test) { return test.type == id; });
	return known ? std::optional<int>{id} : std::nullopt;
}

/// Option table filled by addHealthOptions(): one entry per health option.
using healthOptions = std::unordered_map<healthCmdType, healthCmdStruct>;

/**
 * @brief Declares the health options on @p sub, binding each to its entry in @p opts
 *
 * run() and driverMode() both parse through this so they accept the same command line.
 *
 * @param [in,out] sub  The CLI11 app to add the options to
 * @param [in,out] opts The option table the parsed values are written to
 */
static void addHealthOptions(CLI::App &sub, healthOptions &opts)
{
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", opts[healthCmdType::HEALTH_JSON].enabled, "Print result in JSON format");
	sub.add_flag("-l,--list", opts[healthCmdType::HEALTH_LIST].enabled, "List all available components");
	sub.add_option("-d,--device,--id", opts[healthCmdType::HEALTH_DEVICE].val,
				   "Device index or BDF address, comma-separated for multiple (e.g. 0,1)")
		->each([&opts](const std::string &) { opts[healthCmdType::HEALTH_DEVICE].enabled = true; });
	sub.add_option("-c,--component", opts[healthCmdType::HEALTH_COMPONENT].val, "Component type ID")
		->each([&opts](const std::string &) { opts[healthCmdType::HEALTH_COMPONENT].enabled = true; });
}

/**
 * @brief Checks the parsed options for errors that run() reports before opening a device
 *
 * run() prints the returned message; driverMode() skips driver initialization when
 * there is one, so the error is reported even where Level Zero cannot initialize.
 *
 * @param [in] opts The parsed option table
 * @return The error message, or std::nullopt if the options can reach a device
 */
static std::optional<std::string> argumentError(const healthOptions &opts)
{
	if (!opts.at(healthCmdType::HEALTH_LIST).enabled && !opts.at(healthCmdType::HEALTH_DEVICE).enabled) {
		return "Wrong argument or unknown operation, run with --help for more information.";
	}
	const auto &componentOpt = opts.at(healthCmdType::HEALTH_COMPONENT);
	if (componentOpt.enabled && !knownComponentId(componentOpt.val)) {
		std::string err =
			"Invalid component ID '" + componentOpt.val + "', run with --help for the list of component IDs.";
		// Earlier releases used 6 for GPU Frequency; point scripts still using it at the new ID.
		if (componentOpt.val == "6") {
			err += " GPU Frequency was component ID 6 in earlier releases; use 4 instead.";
		}
		return err;
	}
	return std::nullopt;
}

/**
 * @brief Prints the health usage, options and the list of valid component IDs (1-4).
 *
 * @param [in] helpType Which part of the help to print (for example FULL_HELP).
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
	helpList.push_back(helpCmd(HEADING, "--device,--id               The device ID or PCI BDF address"));
	helpList.push_back(helpCmd(HEADING, "-c,--component              Component types"));
	helpList.push_back(helpCmd(SUB_HEADING2, "1. GPU Core Temperature"));
	helpList.push_back(helpCmd(SUB_HEADING2, "2. GPU Memory Temperature"));
	helpList.push_back(helpCmd(SUB_HEADING2, "3. GPU Power"));
	helpList.push_back(helpCmd(SUB_HEADING2, "4. GPU Frequency"));

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
	TableBuilder table;
	table.addColumn("Component", 30, Align::Left).addColumn("Value", 60, Align::Left);

	table.addRow("Device ID", std::to_string((*jsonObj)["device_id"].get<int>()));

	for (auto &item : jsonObj->items()) {
		if (item.key() == "device_id") {
			continue;
		}

		table.addSeparator();
		table.addRow("[ " + item.key() + " ]", "");
		for (auto &subitem : item.value().items()) {
			std::string value;
			if (subitem.value().is_string()) {
				value = subitem.value().get<std::string>();
			} else if (subitem.value().is_number_integer() && subitem.value().get<int64_t>() == -1) {
				value = "N/A";
			} else {
				value = subitem.value().dump();
			}
			table.addRow(subitem.key(), value);
		}
	}

	PRINT("{}", table.toString().c_str());
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
	if (jsonObj->contains("error")) {
		PRINT("Error: {}\n", (*jsonObj)["error"].get<std::string>().c_str());
	} else if (jsonObj->contains("device_list")) {
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
 * tests including temperature monitoring, power assessment
 * and frequency subsystem checks. Results are stored in the provided JSON object.
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
			ERR("Health check failed for device id: {}\n", d.index);
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
 * assessment covering core temperature, memory temperature, power
 * and frequency subsystems. Results are stored in the provided JSON object.
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
		// If the device is an iGPU and the test is not supported for iGPUs, skip it
		if (d->dev->isIGPU() && !test.canRunOnIGPU) {
			DBG("Skipping test {} for integrated GPU (BDF: {})\n", test.type, d->dev->getBDFStr());
			continue;
		}
		DBG("Running test: {}\n", test.type);
		result = (this->*test.func)(d, jsonObj);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Health check failed for device id: {}\n", d->index);
		}
	}

	return result;
}
/**
 * @brief Performs comprehensive component health assessment for the device
 *
 * This function executes a series of health checks across various device
 * components including temperatures, power consumption, frequency,
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
	const auto componentId = knownComponentId(healthCmds[healthCmdType::HEALTH_COMPONENT].val);

	for (const auto &test : componentCmds) {
		if (componentId && test.type == *componentId) {
			// If the device is an iGPU and the test is not supported for iGPUs, skip it
			if (d->dev->isIGPU() && !test.canRunOnIGPU) {
				DBG("Skipping test {} for integrated GPU (BDF: {})\n", test.type, d->dev->getBDFStr());
				return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
			}
			DBG("Running test: {}\n", test.type);
			found = true;
			result = (this->*test.func)(d, jsonObj);
			break;
		}
	}

	if (!found) {
		ERR("The following argument was not expected: '{}'.\n",
			healthCmds[healthCmdType::HEALTH_COMPONENT].val.c_str());
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	return result;
}

/**
 * @brief Common function to handle temperature monitoring for both core and memory
 *
 * This function provides common temperature monitoring logic that can be used for
 * both core and memory temperature measurements. It retrieves thresholds, measures
 * temperature values, determines health status, and formats results for JSON output.
 *
 * @param d Pointer to device information structure containing device handles
 * @param jsonObj Pointer to a JSON object where results will be stored
 * @param jsonKey The key name to use in the JSON object (e.g., "core_temperature", "memory_temperature")
 * @param getThresholdFunc Function pointer to retrieve temperature thresholds
 * @param getTempFunc Function pointer to retrieve temperature value
 * @param thresholdErrorMsg Error message for threshold retrieval failure
 * @param tempErrorMsg Error message for temperature retrieval failure
 * @return ze_result_t ZE_RESULT_SUCCESS once the result is recorded, also when the temperature cannot be
 *         read; an error code when there is no temperature object or the thresholds cannot be read
 * @note A device without the sensor is reported as an error with @p tempErrorMsg. A sensor that
 *       exists but cannot be read leaves the status unknown and is logged only at debug level.
 */
ze_result_t cmdHealth::getTemperatureHealth(devInfo *d, nlohmann::ordered_json *jsonObj, const std::string &jsonKey,
											ze_result_t (temperature::*getThresholdFunc)(zes_device_handle_t,
																						 uint32_t *, uint32_t *),
											ze_result_t (temperature::*getTempFunc)(double *),
											const std::string &thresholdErrorMsg, const std::string &tempErrorMsg)
{
	TRACING();
	ze_result_t result = ZE_RESULT_SUCCESS;
	uint32_t throttleThreshold = 0;
	uint32_t shutdownThreshold = 0;

	xpumHealthStatus status = xpumHealthStatus::XPUM_HEALTH_STATUS_UNKNOWN;
	temperature *t = (temperature *)d->dev->getTemperature();
	if (t == nullptr) {
		ERR("Failed to get temperature handle\n");
		setTemperatureFailureJson(jsonObj, jsonKey, "Failed to get temperature handle.");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	result = (t->*getThresholdFunc)(d->zesDeviceHdl, &throttleThreshold, &shutdownThreshold);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("{}: 0x{:X} ({})\n", thresholdErrorMsg.c_str(), result, l0_error_to_string(result));
		setTemperatureFailureJson(jsonObj, jsonKey, thresholdErrorMsg + ": " + l0_error_to_string(result));
		return result;
	}

	double tempVal = 0;
	std::string description = "Health cannot be determined for temperature sensors.";
	result = (t->*getTempFunc)(&tempVal);
	if (result != ZE_RESULT_SUCCESS && result != ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
		// The sensor exists but could not be read, as happens without root: health stays undetermined.
		DBG("{}: 0x{:X} ({})\n", tempErrorMsg.c_str(), result, l0_error_to_string(result));
	} else if (result != ZE_RESULT_SUCCESS) {
		ERR("{}: 0x{:X} ({})\n", tempErrorMsg.c_str(), result, l0_error_to_string(result));
		description = tempErrorMsg + ": " + l0_error_to_string(result);
	}
	if (tempVal > 0 && tempVal < throttleThreshold) {
		status = xpumHealthStatus::XPUM_HEALTH_STATUS_OK;
		description = "Values for all the temperature sensors are healthy.";
	}
	if (tempVal >= throttleThreshold) {
		status = xpumHealthStatus::XPUM_HEALTH_STATUS_WARNING;
		std::stringstream tempBuffer;
		tempBuffer << std::fixed << std::setprecision(2) << tempVal;
		description = "Unhealthy temperature sensor value, " + tempBuffer.str() +
					  " >= " + std::to_string(throttleThreshold) + " (threshold)";
	}
	(*jsonObj)[jsonKey] = {{"status", getHealthStatusString(status)},
						   {"throttle_threshold", throttleThreshold},
						   {"shutdown_threshold", shutdownThreshold},
						   {"description", description}};

	return ZE_RESULT_SUCCESS;
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
	return getTemperatureHealth(d, jsonObj, "core_temperature", &temperature::getCoreThreshold,
								&temperature::getCoreTemp, "Failed to get core temperature thresholds",
								"Failed to get core temperature");
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
	return getTemperatureHealth(d, jsonObj, "memory_temperature", &temperature::getMemoryThreshold,
								&temperature::getMemoryTemp, "Failed to get memory temperature thresholds",
								"Failed to get memory temperature");
}

/**
 * @brief Performs GPU power subsystem health assessment
 * Compares maximum value of all the device power domains (and sum of subdevice
 * power domain values) against power threshold.  Results in OK state for values
 * below threshold, and a warning otherwise. Result is stored in the provided JSON
 *
 * @param d Pointer to device information structure (currently unused)
 * @param jsonObj Pointer to a JSON object where results will be stored
 * @return ze_result_t ZE_RESULT_SUCCESS indicating successful assessment
 */
ze_result_t cmdHealth::gpuPower(devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();
	std::string pciDeviceId;

	std::string description = "Health cannot be determined for the power domains.";
	xpumHealthStatus status = xpumHealthStatus::XPUM_HEALTH_STATUS_UNKNOWN;
	ze_device_properties_t zeDevProp = {};
	ze_result_t res;

	power *pwr = d->dev->getPower();
	if (pwr == nullptr) {
		ERR("Failed to get power handle\n");
		setHealthFailureJson(jsonObj, "power_health", "Failed to get power handle.");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	uint32_t deviceId = 0;
	res = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (res == ZE_RESULT_SUCCESS) {
		deviceId = zeDevProp.deviceId;
	} else {
		// zeDeviceGetProperties unavailable; zesDeviceGetProperties exposes the
		// same deviceId via the embedded core struct.
		zes_device_properties_t zesProps{};
		const ze_result_t zesRes = d->dev->zesGetDevProps(d->zesDeviceHdl, &zesProps);
		if (zesRes == ZE_RESULT_SUCCESS) {
			deviceId = zesProps.core.deviceId;
		} else {
			ERR("Failed to get device properties: 0x{:X} ({})\n", zesRes, l0_error_to_string(zesRes));
			setHealthFailureJson(jsonObj, "power_health",
								 std::string("Failed to get device properties: ") + l0_error_to_string(zesRes));
			return zesRes;
		}
	}

	auto powerThreshold = pwr->getThrottlePower(deviceId);
	if (powerThreshold <= 0) {
		description = "Health threshold for the power domains is not set";
		ERR("Power threshold is not set for power domain\n");
		setHealthFailureJson(jsonObj, "power_health", description);
		return ZE_RESULT_NOT_READY;
	}

	uint32_t powerDomainCount = pwr->getPowerCount();
	zes_pwr_handle_t *powerHandles = pwr->getPowerHandles();
	if (powerDomainCount > 0 && powerHandles != nullptr) {
		uint64_t currentDeviceMaxDomainValue = 0;
		uint64_t currentSubDeviceValueSum = 0;
		for (uint32_t i = 0; i < powerDomainCount; ++i) {
			zes_power_properties_t props = {};
			zes_power_ext_properties_t extProps = {};
			extProps.pNext = nullptr;
			res = pwr->getProperties(powerHandles[i], &props, &extProps);
			if (res != ZE_RESULT_SUCCESS) {
				continue;
			}
			zes_power_energy_counter_t snap1 = {};
			zes_power_energy_counter_t snap2 = {};
			res = pwr->getEnergyCounter(powerHandles[i], &snap1);
			if (res == ZE_RESULT_SUCCESS) {
				xpum::watchdog::sleepFor(std::chrono::milliseconds(POWER_MONITOR_INTERNAL_PERIOD));
				res = pwr->getEnergyCounter(powerHandles[i], &snap2);
				if (res == ZE_RESULT_SUCCESS && snap2.timestamp != snap1.timestamp) {
					uint64_t value = (snap2.energy - snap1.energy) / (snap2.timestamp - snap1.timestamp);
					if (props.onSubdevice) {
						currentSubDeviceValueSum += value;
					} else if (value > currentDeviceMaxDomainValue) {
						currentDeviceMaxDomainValue = value;
					}
				}
			}
		}
		DBG("health: current device max domain power value: {}", currentDeviceMaxDomainValue);
		DBG("health: current sum of sub-device power values: {}", currentSubDeviceValueSum);
		auto powerVal = std::max(currentDeviceMaxDomainValue, currentSubDeviceValueSum);
		if (powerVal < powerThreshold && status < xpumHealthStatus::XPUM_HEALTH_STATUS_OK) {
			status = xpumHealthStatus::XPUM_HEALTH_STATUS_OK;
			description = "Values for all the power domains are healthy.";
		}
		if (powerVal >= powerThreshold && status < xpumHealthStatus::XPUM_HEALTH_STATUS_WARNING) {
			status = xpumHealthStatus::XPUM_HEALTH_STATUS_WARNING;
			description = "Unhealthy power domain value, " + std::to_string(powerVal) +
						  " >= " + std::to_string(powerThreshold) + " (threshold)";
		}
	}

	(*jsonObj)["power_health"] = {{"status", getHealthStatusString(status)}, {"description", description}};

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Converts XPUM health status enum to human-readable string
 *
 * This function converts XPUM health status enumeration values to descriptive
 * string representations suitable for display in user interfaces and reports.
 * It provides consistent string formatting across the health monitoring system.
 *
 * @param status The XPUM health status enumeration value to convert
 * @return std::string Human-readable health status string ("OK", "Warning", "Critical", "Unknown").
 *         TitleCase is intentional: it matches the documented health JSON
 *         schema enum.
 */
std::string cmdHealth::getHealthStatusString(xpumHealthStatus status)
{
	switch (status) {
	case XPUM_HEALTH_STATUS_OK:
		return "OK";
	case XPUM_HEALTH_STATUS_WARNING:
		return "Warning";
	case XPUM_HEALTH_STATUS_CRITICAL:
		return "Critical";
	case XPUM_HEALTH_STATUS_UNKNOWN:
		return "Unknown";
	default:
		return "Unknown";
	}
}

/**
 * @brief Converts frequency throttle reason flags to descriptive string
 *
 * This function analyzes frequency throttle reason flags from the ZES API
 * and converts them into human-readable descriptions. It helps identify why
 * the GPU frequency is being throttled, which is crucial for performance
 * analysis and thermal management diagnostics.
 *
 * @param flags Bitmask of ZES frequency throttle reason flags
 * @return std::string Descriptive text explaining (one of) the throttling reason(s)
 */
#define THROTTLE_FLAG(flags, flag) ((flags & flag) == flag)

std::string cmdHealth::getFreqThrottleString(zes_freq_throttle_reason_flags_t flags)
{
	if (THROTTLE_FLAG(flags, ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP))
		return std::string("frequency throttled due to average power excursion.");
	if (THROTTLE_FLAG(flags, ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP))
		return std::string("frequency throttled due to burst power excursion.");
	if (THROTTLE_FLAG(flags, ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT))
		return std::string("frequency throttled due to current excursion.");
	if (THROTTLE_FLAG(flags, ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT))
		return std::string("frequency throttled due to thermal excursion.");
	if (THROTTLE_FLAG(flags, ZES_FREQ_THROTTLE_REASON_FLAG_PSU_ALERT))
		return std::string("frequency throttled due to power supply assertion.");
	if (THROTTLE_FLAG(flags, ZES_FREQ_THROTTLE_REASON_FLAG_SW_RANGE))
		return std::string("frequency throttled due to software supplied frequency range.");
	if (THROTTLE_FLAG(flags, ZES_FREQ_THROTTLE_REASON_FLAG_HW_RANGE))
		return std::string("frequency throttled due to a sub block that has a lower frequency.");

	return std::string("frequency throttled reason cannot be determined.");
}

#undef THROTTLE_FLAG

/**
 * @brief Retrieves the current frequency state and throttling information
 *
 * This function queries all frequency domains on the device to determine
 * if any frequency throttling is occurring. It examines both device-level
 * and tile-level frequency domains, building a comprehensive message about
 * any active throttling conditions. The function prioritizes device-level
 * throttling over tile-level throttling in its reporting.
 *
 * @param device Handle to the ZES device to query
 * @param freqThrottleMessage Reference to string that will contain throttling description
 * @return bool True if frequency state was successfully retrieved, false on error
 */
bool cmdHealth::getFrequencyState(const zes_device_handle_t &device, std::string &freqThrottleMessage)
{
	bool ret = false;
	if (device == nullptr) {
		return ret;
	}

	// Create a temporary frequency instance to enumerate and query frequency domains.
	// The caller only passes a device handle, so we re-enumerate here rather than
	// accessing the device object's frequency instance directly.

	::frequency freqHelper;
	ze_result_t initResult = freqHelper.init(device);
	if (initResult != ZE_RESULT_SUCCESS) {
		return ret;
	}

	uint32_t freqCount = freqHelper.getFrequencyCount();
	auto freqHandles = freqHelper.getFrequencyHandles();
	if (freqCount == 0 || freqHandles.empty()) {
		return ret;
	}

	for (uint32_t i = 0; i < freqCount; ++i) {
		zes_freq_properties_t props = {};
		props.pNext = nullptr;
		ze_result_t res = freqHelper.getProperties(freqHandles[i], &props);
		if (res == ZE_RESULT_SUCCESS) {
			zes_freq_state_t freqState = {};
			res = freqHelper.getState(freqHandles[i], &freqState);
			if (res == ZE_RESULT_SUCCESS) {
				if (freqState.throttleReasons == 0) {
					ret = true;
					continue;
				}
				if (props.onSubdevice) {
					if (!freqThrottleMessage.empty())
						freqThrottleMessage += " ";
					freqThrottleMessage += "Tile " + std::to_string(props.subdeviceId) + " " +
										   this->getFreqThrottleString(freqState.throttleReasons);
					ret = true;
				} else {
					freqThrottleMessage = "Device " + this->getFreqThrottleString(freqState.throttleReasons);
					return true;
				}
			}
		}
	}

	return ret;
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
	xpumHealthStatus status = xpumHealthStatus::XPUM_HEALTH_STATUS_UNKNOWN;
	std::string description = "Device frequency state cannot be determined.";
	std::string freqThrottleMessage;
	bool getState = this->getFrequencyState(d->zesDeviceHdl, freqThrottleMessage);
	if (getState) {
		if (!freqThrottleMessage.empty()) {
			status = xpumHealthStatus::XPUM_HEALTH_STATUS_WARNING;
			description = freqThrottleMessage;
		} else {
			status = xpumHealthStatus::XPUM_HEALTH_STATUS_OK;
			description = "Device frequency is not throttled";
		}
	}

	(*jsonObj)["frequency_health"] = {{"status", getHealthStatusString(status)}, {"description", description}};

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Only a command line that run() can take to a device needs the driver.
 *
 * A bare `health`, a parse error and the argument errors from argumentError()
 * are all reported by run() before any device is opened, so they must not fail
 * first on driver initialization.
 *
 * @param[in] cmdArgs  argv tokens after the subcommand name.
 *
 * @retval DriverMode::None  @p cmdArgs is empty, fails to parse, or is rejected by argumentError().
 * @retval DriverMode::Full  Otherwise.
 */
DriverMode cmdHealth::driverMode(std::span<char *const> cmdArgs) const
{
	if (printsHelpOnly(cmdArgs)) {
		return DriverMode::None;
	}
	healthOptions opts = {{healthCmdType::HEALTH_JSON, {}},
						  {healthCmdType::HEALTH_LIST, {}},
						  {healthCmdType::HEALTH_DEVICE, {}},
						  {healthCmdType::HEALTH_COMPONENT, {}}};
	CLI::App sub{"Check GPU component health", "health"};
	addHealthOptions(sub, opts);
	// CLI11 takes a token vector in reverse order.
	std::vector<std::string> tokens(cmdArgs.rbegin(), cmdArgs.rend());
	try {
		sub.parse(std::move(tokens));
	} catch (const CLI::Error &) {
		return DriverMode::None;
	}
	return argumentError(opts) ? DriverMode::None : DriverMode::Full;
}

/**
 * @brief Executes the health command.
 *
 * Parses the options with addHealthOptions(), reports argumentError() failures
 * (missing -d/-l, unknown component ID) before any device is opened, then prints
 * health for the selected devices and component.
 *
 * @param [in,out] args Parsed program arguments and the Sysman driver instance.
 *
 * @retval ZE_RESULT_SUCCESS                Health was printed, or help was requested.
 * @retval ZE_RESULT_ERROR_INVALID_ARGUMENT Bad command line or unknown component ID.
 * @retval other                            Device lookup or health query failed.
 */
int cmdHealth::run(arg_struct *args)
{
	TRACING();
	std::vector<devInfo> deviceList;
	ze_result_t result;
	std::unique_ptr<Printer> printer;

	// Reset state
	for (auto &[k, v] : healthCmds) {
		v.enabled = false;
		v.val.clear();
	}

	CLI::App sub{"Check GPU component health", "health"};
	addHealthOptions(sub, healthCmds);

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

	// If no options were specified, print help
	if (printsHelpOnly(subcommandArgs(*args))) {
		help();
		return ZE_RESULT_SUCCESS;
	}

	if (healthCmds[healthCmdType::HEALTH_JSON].enabled == true) {
		printer = std::make_unique<JsonPrinter>();
	} else {
		printer = std::make_unique<HealthTextPrinter>();
	}
	auto jsonObj = std::make_unique<nlohmann::ordered_json>();
	// A missing -d/-l or an unknown component ID is rejected before any device is
	// opened; driverMode() skips driver init for these, so this runs without Level Zero.
	if (const auto err = argumentError(healthCmds)) {
		result = ZE_RESULT_ERROR_INVALID_ARGUMENT;
		DBG("Error: {}\n", err->c_str());
		(*jsonObj)["error"] = *err;
		(*jsonObj)["errno"] = result;
		printer->print(jsonObj.get());
		return result;
	}

	// Find the device based on the provided device ID or PCI BDF address
	result = args->sm.findDevice(healthCmds[healthCmdType::HEALTH_DEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS || deviceList.empty()) {
		DBG("Error: Device handle not found for device ID '{}'.\n",
			healthCmds[healthCmdType::HEALTH_DEVICE].val.c_str());
		(*jsonObj)["error"] = "device not found";
		(*jsonObj)["errno"] = ZE_RESULT_ERROR_INVALID_ARGUMENT;
		printer->print(jsonObj.get());
		return result;
	}

	if (healthCmds[healthCmdType::HEALTH_LIST].enabled) {
		// List all devices
		result = this->allComponentsAllDevices(&deviceList, jsonObj.get());
		if (result != ZE_RESULT_SUCCESS) {
			std::string err = "Unable to retrieve health info for devices.";
			DBG("Error: {}\n", err.c_str());
			(*jsonObj)["error"] = err;
			(*jsonObj)["errno"] = result;
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

	printer->print(jsonObj.get());

	return result;
}
