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
#include <CLI/CLI.hpp>
#include <temperature.h>
#include <memory.h>
#include <sstream>
#include <iomanip>
#include <thread>
#include <inttypes.h>

static std::unordered_map<healthCmdType, healthCmdStruct> healthCmds = {
	{healthCmdType::HEALTH_HELP, {}},
	{healthCmdType::HEALTH_JSON, {}},
	{healthCmdType::HEALTH_LIST, {}},
	{healthCmdType::HEALTH_DEVICE, {.func = &cmdHealth::allComponents}},
	{healthCmdType::HEALTH_COMPONENT, {.func = &cmdHealth::component}},
};

healthSubCmdStruct componentCmds[] = {
	{healthSubCmdType::HEALTH_CORETEMPERATURE, &cmdHealth::coreTemperature},
	{healthSubCmdType::HEALTH_MEMORYTEMPERATURE, &cmdHealth::memoryTemperature},
	{healthSubCmdType::HEALTH_POWER, &cmdHealth::gpuPower},
	{healthSubCmdType::HEALTH_MEMORY, &cmdHealth::healthMemory},
	{healthSubCmdType::HEALTH_UNSUPPORTED0, &cmdHealth::unsupported},
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
	helpList.push_back(helpCmd(HEADING, "--device,--id               The device ID or PCI BDF address"));
	helpList.push_back(helpCmd(HEADING, "-c,--component              Component types"));
	helpList.push_back(helpCmd(SUB_HEADING2, "1. GPU Core Temperature"));
	helpList.push_back(helpCmd(SUB_HEADING2, "2. GPU Memory Temperature"));
	helpList.push_back(helpCmd(SUB_HEADING2, "3. GPU Power"));
	helpList.push_back(helpCmd(SUB_HEADING2, "4. GPU Memory"));
	helpList.push_back(helpCmd(SUB_HEADING2, "5. Unsupported"));
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
 * tests including temperature monitoring, power assessment, memory health evaluation
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
 * assessment covering core temperature, memory temperature, power, memory health
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
		DBG("Running test: {}\n", test.type);
		result = (this->*test.func)(d, jsonObj);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Health check failed for device id: {}\n", d->index);
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
 * @return ze_result_t ZE_RESULT_SUCCESS on success, error code otherwise
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
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	result = (t->*getThresholdFunc)(d->zesDeviceHdl, &throttleThreshold, &shutdownThreshold);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("{}: 0x{:X} ({})\n", thresholdErrorMsg.c_str(), result, l0_error_to_string(result));
		return result;
	}

	double tempVal = 0;
	std::string description = "Health cannot be determined for temperature sensors.";
	result = (t->*getTempFunc)(&tempVal);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("{}: 0x{:X} ({})\n", tempErrorMsg.c_str(), result, l0_error_to_string(result));
		return result;
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
	res = d->dev->getDevProps(d->deviceHdl, &zeDevProp);
	if (res != ZE_RESULT_SUCCESS) {
		ERR("Failed to get device properties: 0x{:X} ({})\n", res, l0_error_to_string(res));
		return res;
	}

	auto powerThreshold = pwr->getThrottlePower(zeDevProp.deviceId);
	if (powerThreshold <= 0) {
		description = "Health threshold for the power domains is not set";
		ERR("Power threshold is not set for power domain\n");
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
				std::this_thread::sleep_for(std::chrono::milliseconds(POWER_MONITOR_INTERNAL_PERIOD));
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
 * @brief Converts memory health enum to human-readable string
 *
 * This function converts ZES memory health enumeration values to descriptive
 * string representations for display purposes.
 *
 * @param health The ZES memory health enumeration value
 * @return std::string Human-readable health status string
 */
xpumHealthStatus cmdHealth::getHealthStatus(zes_mem_health_t health)
{
	if (health == ZES_MEM_HEALTH_OK) {
		return xpumHealthStatus::XPUM_HEALTH_STATUS_OK;
	} else if (health == ZES_MEM_HEALTH_DEGRADED) {
		return xpumHealthStatus::XPUM_HEALTH_STATUS_WARNING;
	} else if (health == ZES_MEM_HEALTH_CRITICAL) {
		return xpumHealthStatus::XPUM_HEALTH_STATUS_CRITICAL;
	} else if (health == ZES_MEM_HEALTH_REPLACE) {
		return xpumHealthStatus::XPUM_HEALTH_STATUS_CRITICAL;
	} else {
		return xpumHealthStatus::XPUM_HEALTH_STATUS_UNKNOWN;
	}
}

/**
 * @brief Converts XPUM health status enum to human-readable string
 *
 * This function converts XPUM health status enumeration values to descriptive
 * string representations suitable for display in user interfaces and reports.
 * It provides consistent string formatting across the health monitoring system.
 *
 * @param status The XPUM health status enumeration value to convert
 * @return std::string Human-readable health status string ("OK", "WARNING", "CRITICAL", "UNKNOWN")
 */
std::string cmdHealth::getHealthStatusString(xpumHealthStatus status)
{
	switch (status) {
	case XPUM_HEALTH_STATUS_OK:
		return "OK";
	case XPUM_HEALTH_STATUS_WARNING:
		return "WARNING";
	case XPUM_HEALTH_STATUS_CRITICAL:
		return "CRITICAL";
	case XPUM_HEALTH_STATUS_UNKNOWN:
		return "UNKNOWN";
	default:
		return "UNKNOWN";
	}
}

/**
 * @brief Converts ZES memory health enum to descriptive text
 *
 * This function maps Level Zero Sysman (ZES) memory health enumeration values
 * to detailed descriptive text strings that explain the specific memory health
 * condition. These descriptions provide users with actionable information about
 * memory subsystem status and recommended actions.
 * link: https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/PROG.html#querying-memory-modules
 *
 * @param val The ZES memory health enumeration value to describe
 * @return std::string Detailed description of the memory health condition
 */
std::string cmdHealth::getHealthStatusDescription(zes_mem_health_t val)
{
	switch (val) {
	case zes_mem_health_t::ZES_MEM_HEALTH_UNKNOWN:
		return std::string("The memory health cannot be determined.");
	case zes_mem_health_t::ZES_MEM_HEALTH_OK:
		return std::string("All memory channels are healthy.");
	case zes_mem_health_t::ZES_MEM_HEALTH_DEGRADED:
		return std::string(
			"Excessive correctable errors have been detected on one or more channels. Device should be reset.");
	case zes_mem_health_t::ZES_MEM_HEALTH_CRITICAL:
		return std::string("Operating with reduced memory to cover banks with too many uncorrectable errors.");
	case zes_mem_health_t::ZES_MEM_HEALTH_REPLACE:
		return std::string("Device should be replaced due to excessive uncorrectable errors.");
	default:
		return std::string("The memory health cannot be determined.");
	}
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
		ERR("Failed to get memory temperature thresholds: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	xpumHealthStatus healthStatus = this->getHealthStatus(health);

	(*jsonObj)["memory_health"] = {{"status", getHealthStatusString(healthStatus)},
								   {"description", getHealthStatusDescription(health)}};

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Performs unsupported feature health assessment
 *
 * This function evaluates the health status of unsupported item.
 *
 * @param d Pointer to device information structure (currently unused)
 * @param jsonObj Pointer to a JSON object where results will be stored
 * @return ze_result_t ZE_RESULT_SUCCESS indicating successful assessment
 */
ze_result_t cmdHealth::unsupported(UNUSED devInfo *d, nlohmann::ordered_json *jsonObj)
{
	TRACING();
	std::string description = "Unsupported.";
	xpumHealthStatus status = xpumHealthStatus::XPUM_HEALTH_STATUS_UNKNOWN;

	(*jsonObj)["unsupported_item_health"] = {{"status", getHealthStatusString(status)}, {"description", description}};

	return ZE_RESULT_SUCCESS;
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
 * @brief Executes the health run.
 *
 * @return int Returns 0 on success.
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
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", healthCmds[healthCmdType::HEALTH_JSON].enabled, "Print result in JSON format");
	sub.add_flag("-l,--list", healthCmds[healthCmdType::HEALTH_LIST].enabled, "List all available components");
	sub.add_option("-d,--device,--id", healthCmds[healthCmdType::HEALTH_DEVICE].val, "Device ID or PCI BDF address")
		->each([&](const std::string &) { healthCmds[healthCmdType::HEALTH_DEVICE].enabled = true; });
	sub.add_option("-c,--component", healthCmds[healthCmdType::HEALTH_COMPONENT].val, "Component type ID")
		->each([&](const std::string &) { healthCmds[healthCmdType::HEALTH_COMPONENT].enabled = true; });

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
	if (args->argc == 2) {
		help();
		return ZE_RESULT_SUCCESS;
	}

	if (healthCmds[healthCmdType::HEALTH_JSON].enabled == true) {
		printer = std::make_unique<JsonPrinter>();
	} else {
		printer = std::make_unique<HealthTextPrinter>();
	}
	auto jsonObj = std::make_unique<nlohmann::ordered_json>();
	// user must specify a device ID or PCI BDF address
	if (!healthCmds[healthCmdType::HEALTH_LIST].enabled && !healthCmds[healthCmdType::HEALTH_DEVICE].enabled) {
		result = ZE_RESULT_ERROR_INVALID_ARGUMENT;
		std::string err = "Wrong argument or unknown operation, run with --help for more information.";
		DBG("Error: {}\n", err.c_str());
		(*jsonObj)["error"] = err;
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
