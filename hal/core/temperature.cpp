/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "temperature.h"
#include "debug.h"
#include <cmath>
#include <map>
#include <memory>
#include <vector>

/**
 * @brief Helper function to parse device ID from hexadecimal string
 *
 * @param hexKey Hexadecimal string representation of device ID
 * @return uint32_t Parsed device ID
 */
uint32_t temperature::parseDeviceId(const std::string &hexKey) { return (uint32_t)std::stoul(hexKey, nullptr, 16); }

/**
 * @brief Helper function to load threshold data from JSON into a map
 *
 * This function checks if the JSON object contains the specified key,
 * then iterates through the device-specific thresholds and calls the setter function.
 *
 * @param thresholdsJson JSON object containing threshold configurations
 * @param key The key to look for in the JSON (e.g., "throttle_core", "shutdown_core")
 * @param setter Function to call for each device ID and threshold value pair
 */
void temperature::loadThresholdSection(const nlohmann::json &thresholdsJson, const std::string &key,
									   std::function<void(uint32_t, uint64_t)> setter)
{
	if (thresholdsJson.contains(key)) {
		auto section = thresholdsJson[key];
		for (auto it = section.begin(); it != section.end(); ++it) {
			try {
				uint32_t deviceId = parseDeviceId(it.key());
				uint64_t threshold = it.value().get<uint64_t>();
				setter(deviceId, threshold);
			} catch (const std::exception &e) {
				ERR("Error parsing {} threshold for device {}: {}\n", key.c_str(), it.key().c_str(), e.what());
			}
		}
	}
}

/**
 * @brief Loads temperature threshold configuration from JSON file
 *
 * This function reads temperature thresholds from the configuration file
 * device_thresholds.json and populates the device-specific threshold maps.
 * If the file cannot be read or parsed, default values are used.
 */
void temperature::loadTemperatureThresholds()
{
	try {
		std::ifstream configFile(std::string(XPUM_CONFIG_DIR) + std::string("device_thresholds.json"));

		if (configFile.is_open()) {
			nlohmann::json configJson;
			configFile >> configJson;
			configFile.close();

			if (configJson.contains("default_thresholds")) {
				auto defaults = configJson["default_thresholds"];
				if (defaults.contains("core_throttle")) {
					defaultCoreThrottleThreshold = defaults["core_throttle"].get<uint64_t>();
				}
				if (defaults.contains("core_shutdown")) {
					defaultCoreShutdownThreshold = defaults["core_shutdown"].get<uint64_t>();
				}
				if (defaults.contains("memory_shutdown")) {
					defaultMemoryShutdownThreshold = defaults["memory_shutdown"].get<uint64_t>();
				}
			}

			if (configJson.contains("temperature_thresholds")) {
				auto thresholdsJson = configJson["temperature_thresholds"];

				// Load throttle core temperatures
				loadThresholdSection(thresholdsJson, "throttle_core", [this](uint32_t deviceId, uint64_t value) {
					thresholds->setThrottleCoreTemperature(deviceId, value);
				});

				// Load shutdown core temperatures
				loadThresholdSection(thresholdsJson, "shutdown_core", [this](uint32_t deviceId, uint64_t value) {
					thresholds->setShutdownCoreTemperature(deviceId, value);
				});

				// Load shutdown memory temperatures
				loadThresholdSection(thresholdsJson, "shutdown_memory", [this](uint32_t deviceId, uint64_t value) {
					thresholds->setShutdownMemoryTemperature(deviceId, value);
				});
			}
		} else {
			DBG("Temperature thresholds config file not found, using defaults\n");
		}
	} catch (const std::exception &e) {
		ERR("Error loading temperature thresholds config: {}\n", e.what());
		DBG("Using default temperature thresholds\n");
	}
}

/**
 * @brief Constructor for the temperature class
 *
 * Initializes the temperature management object with default values and
 * loads temperature thresholds from configuration.
 */
temperature::temperature()
	: temperatureCount(0), temperatureHandles(nullptr), thresholds(new TemperatureThresholds()),
	  defaultCoreThrottleThreshold(CORE_THROTTLE_THRESHOLD_DEFAULT),
	  defaultCoreShutdownThreshold(CORE_SHUTDOWN_THRESHOLD_DEFAULT),
	  defaultMemoryShutdownThreshold(MEMORY_SHUTDOWN_THRESHOLD_DEFAULT), hasLpddr5Memory(false)
{
	loadTemperatureThresholds();
}

/**
 * @brief Destructor for the temperature class
 *
 * This destructor performs cleanup operations for the temperature management
 * object, releasing allocated memory for temperature sensor handles and ensuring
 * proper resource deallocation when the temperature object is destroyed.
 */
temperature::~temperature()
{
	if (temperatureHandles) {
		delete[] temperatureHandles;
		temperatureHandles = nullptr;
	}
	delete thresholds;
	thresholds = nullptr;
}

/**
 * @brief TemperatureThresholds getter methods
 */
uint64_t TemperatureThresholds::getThrottleCoreTemperature(uint32_t deviceId, uint64_t defaultValue) const
{
	auto it = pHealthDeviceToThrottleCoreTemperatures.find(deviceId);
	return (it != pHealthDeviceToThrottleCoreTemperatures.end()) ? it->second : defaultValue;
}

uint64_t TemperatureThresholds::getShutdownCoreTemperature(uint32_t deviceId, uint64_t defaultValue) const
{
	auto it = pHealthDeviceToShutdownCoreTemperatures.find(deviceId);
	return (it != pHealthDeviceToShutdownCoreTemperatures.end()) ? it->second : defaultValue;
}

uint64_t TemperatureThresholds::getShutdownMemoryTemperature(uint32_t deviceId, uint64_t defaultValue) const
{
	auto it = pHealthDeviceToShutdownMemoryTemperatures.find(deviceId);
	return (it != pHealthDeviceToShutdownMemoryTemperatures.end()) ? it->second : defaultValue;
}

/**
 * @brief TemperatureThresholds setter methods
 */
void TemperatureThresholds::setThrottleCoreTemperature(uint32_t deviceId, uint64_t value)
{
	pHealthDeviceToThrottleCoreTemperatures[deviceId] = value;
}

void TemperatureThresholds::setShutdownCoreTemperature(uint32_t deviceId, uint64_t value)
{
	pHealthDeviceToShutdownCoreTemperatures[deviceId] = value;
}

void TemperatureThresholds::setShutdownMemoryTemperature(uint32_t deviceId, uint64_t value)
{
	pHealthDeviceToShutdownMemoryTemperatures[deviceId] = value;
}

/**
 * @brief Gets the throttle temperature threshold for GPU core
 *
 * This function retrieves the temperature threshold at which GPU core throttling
 * begins for thermal protection. Returns a device-specific threshold if configured,
 * otherwise returns the default throttle temperature of 105°C.
 *
 * @param pciDeviceId PCI device ID to lookup device-specific threshold
 * @return uint64_t Throttle temperature threshold in Celsius
 */
uint64_t temperature::getThrottleCoreTemperature(uint32_t pciDeviceId)
{
	return thresholds->getThrottleCoreTemperature(pciDeviceId, defaultCoreThrottleThreshold);
}

/**
 * @brief Gets the shutdown temperature threshold for GPU core
 *
 * This function retrieves the critical temperature threshold at which the GPU core
 * will be shut down for thermal protection. Returns a device-specific threshold
 * if configured, otherwise returns the default shutdown temperature of 130°C.
 *
 * @param pciDeviceId PCI device ID to lookup device-specific threshold
 * @return uint64_t Shutdown temperature threshold in Celsius
 */
uint64_t temperature::getShutdownCoreTemperature(uint32_t pciDeviceId)
{
	return thresholds->getShutdownCoreTemperature(pciDeviceId, defaultCoreShutdownThreshold);
}

/**
 * @brief Gets the shutdown temperature threshold for memory
 *
 * This function retrieves the critical temperature threshold at which the memory
 * subsystem will be shut down for thermal protection. Returns a device-specific
 * threshold if configured, otherwise returns the default shutdown temperature of 100°C.
 *
 * @param pciDeviceId PCI device ID to lookup device-specific threshold
 * @return uint64_t Memory shutdown temperature threshold in Celsius
 */
uint64_t temperature::getShutdownMemoryTemperature(uint32_t pciDeviceId)
{
	return thresholds->getShutdownMemoryTemperature(pciDeviceId, defaultMemoryShutdownThreshold);
}

/**
 * @brief Enumerates available temperature sensor domains for a device
 *
 * This function discovers and catalogs all temperature sensors available on the
 * specified device. Temperature sensors monitor thermal conditions across
 * different components and regions of the device.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
ze_result_t temperature::enumTemperatureDomains(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumTemperatureSensors(device, &temperatureCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || temperatureCount == 0) {
		ERR("Failed to enumerate temperature domains. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	temperatureHandles = new zes_temp_handle_t[temperatureCount];
	result = zesDeviceEnumTemperatureSensors(device, &temperatureCount, temperatureHandles);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get temperature domains. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Found {} temperature domains.\n", temperatureCount);
	return result;
}

/**
 * @brief Gets properties for a specific temperature sensor
 *
 * This function retrieves detailed properties and capabilities for a
 * specific temperature sensor, including sensor type and critical thresholds.
 *
 * @param temperatureHandle Handle to the specific temperature sensor
 * @param properties Pointer to structure to store temperature sensor properties
 * @return ze_result_t ZE_RESULT_SUCCESS on successful property retrieval, error code otherwise
 */
ze_result_t temperature::getProperties(zes_temp_handle_t temperatureHandle, zes_temp_properties_t *properties)
{
	ze_result_t result = zesTemperatureGetProperties(temperatureHandle, properties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get properties for temperature domain 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Temperature Properties:\n");
	DBG("  onSubdevice: {}\n", properties->onSubdevice);
	DBG("  subdeviceId: {}\n", properties->subdeviceId);
	switch (properties->type) {
	case ZES_TEMP_SENSORS_GLOBAL:
		DBG("  type: Global\n");
		break;
	case ZES_TEMP_SENSORS_GPU:
		DBG("  type: GPU\n");
		break;
	case ZES_TEMP_SENSORS_MEMORY:
		DBG("  type: Memory\n");
		break;
	case ZES_TEMP_SENSORS_GLOBAL_MIN:
		DBG("  type: Global Min\n");
		break;
	case ZES_TEMP_SENSORS_GPU_MIN:
		DBG("  type: GPU Min\n");
		break;
	case ZES_TEMP_SENSORS_MEMORY_MIN:
		DBG("  type: Memory Min\n");
		break;
	case ZES_TEMP_SENSORS_GPU_BOARD:
		DBG("  type: GPU Board\n");
		break;
	case ZES_TEMP_SENSORS_GPU_BOARD_MIN:
		DBG("  type: GPU Board Min\n");
		break;
	default:
		DBG("  type: Unknown ({})\n", properties->type);
		break;
	}
	DBG("  maxTemperature: {:f} C\n", properties->maxTemperature);
	DBG("  isCriticalTempSupported: {}\n", properties->isCriticalTempSupported);
	DBG("  isThreshold1Supported: {}\n", properties->isThreshold1Supported);
	DBG("  isThreshold2Supported: {}\n", properties->isThreshold2Supported);

	return result;
}

/**
 * @brief Gets the current temperature state for a specific sensor
 *
 * This function retrieves the current temperature reading from a specific
 * temperature sensor, providing real-time thermal monitoring capabilities
 * for device safety and performance optimization.
 *
 * @param temperatureHandle Handle to the specific temperature sensor
 * @param temp Pointer to store the current temperature value in Celsius
 * @return ze_result_t ZE_RESULT_SUCCESS on successful temperature retrieval, error code otherwise
 */
ze_result_t temperature::getState(zes_temp_handle_t temperatureHandle, double *temp)
{
	ze_result_t result = zesTemperatureGetState(temperatureHandle, temp);
	if (result != ZE_RESULT_SUCCESS) {
		DBG("Failed to get state for temperature domain 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Temperature State:\n");
	DBG("  Current Temperature: {:f} C\n", *temp);

	return result;
}

/**
 * @brief Gets temperature for a specific sensor type
 *
 * This function searches for temperature sensors matching the specified type
 * and retrieves the current temperature reading, providing targeted thermal
 * monitoring for specific device components.
 *
 * @param type The specific temperature sensor type to query
 * @param coreTemp Pointer to store the temperature value in Celsius
 * @return ze_result_t ZE_RESULT_SUCCESS if temperature retrieved successfully, error code otherwise
 */
ze_result_t temperature::getTemp(zes_temp_sensors_t type, double *temp)
{
	TRACING();
	if (temp == nullptr) {
		ERR("Temperature output pointer is null\n");
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_temp_properties_t properties;
	*temp = 0.0; // Default value if no temperature found
	bool sensorFound = false;

	for (uint32_t i = 0; i < temperatureCount; ++i) {
		result = getProperties(temperatureHandles[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			DBG("Failed to get properties for temperature sensor {}: 0x{:X} ({})\n", i, result,
				l0_error_to_string(result));
			continue;
		}

		if (properties.type == type) {
			sensorFound = true;
			result = getState(temperatureHandles[i], temp);
			if (result == ZE_RESULT_SUCCESS) {
				return result;
			} else {
				DBG("Failed to get state for temperature sensor {} (type {}): 0x{:X} ({})\n", i, type, result,
					l0_error_to_string(result));
			}
		}
	}

	return sensorFound ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

/**
 * @brief Gets temperature for a specific sensor type per tile/subdevice
 *
 * This function enumerates all temperature sensors and returns the current
 * temperature for each subdevice/tile that matches the specified sensor type.
 * Used for per-tile temperature monitoring on multi-tile GPUs.
 *
 * @param [in] type The temperature sensor type to query (GPU, Memory, etc.)
 * @param [out] tileTemperatures Map of tile_id -> current temperature in Celsius
 * @return ze_result_t ZE_RESULT_SUCCESS on successful temperature retrieval
 */
ze_result_t temperature::getTempPerTile(zes_temp_sensors_t type, std::map<uint32_t, double> &tileTemperatures)
{
	TRACING();
	tileTemperatures.clear();

	for (uint32_t i = 0; i < temperatureCount; ++i) {
		zes_temp_properties_t properties = {};
		ze_result_t result = getProperties(temperatureHandles[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			DBG("Failed to get properties for temperature sensor {}\n", i);
			continue;
		}

		if (properties.type != type) {
			continue;
		}

		uint32_t tileId = 0;
		if (properties.onSubdevice) {
			tileId = properties.subdeviceId;
		}

		double temp = 0.0;
		result = getState(temperatureHandles[i], &temp);
		if (result != ZE_RESULT_SUCCESS) {
			DBG("Failed to get state for temperature sensor {}: 0x{:X} ({})\n", i, result, l0_error_to_string(result));
			continue;
		}

		// LPDDR5 reports memory temperature as an MR4 thermal code (0..7);
		// convert to Celsius.
		if (type == ZES_TEMP_SENSORS_MEMORY && hasLpddr5Memory) {
			temp = mr4CodeToCelsius(temp);
		}

		// Filter abnormal temperatures
		if (temp < MAX_REASONABLE_TEMP_CELSIUS) {
			tileTemperatures[tileId] = temp;
			DBG("Tile {} {} temperature: {:.2f} C\n", tileId,
				type == ZES_TEMP_SENSORS_GPU ? "GPU" : (type == ZES_TEMP_SENSORS_MEMORY ? "Memory" : "Other"), temp);
		}
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Gets the current GPU core temperature
 *
 * This function retrieves the current temperature reading from the GPU core
 * temperature sensor, providing essential thermal monitoring for GPU performance
 * and safety management.
 *
 * @param coreTemp Pointer to store the GPU core temperature in Celsius
 * @return ze_result_t ZE_RESULT_SUCCESS if core temperature retrieved successfully, error code otherwise
 */
ze_result_t temperature::getCoreTemp(double *coreTemp)
{
	TRACING();
	return getTemp(ZES_TEMP_SENSORS_GPU, coreTemp);
}

/**
 * @brief Gets the current memory temperature
 *
 * This function retrieves the current temperature reading from the memory
 * temperature sensor, providing thermal monitoring capabilities for memory
 * subsystem safety and performance optimization.
 *
 * @param memTemp Pointer to store the memory temperature in Celsius
 * @return ze_result_t ZE_RESULT_SUCCESS if memory temperature retrieved successfully, error code otherwise
 */
ze_result_t temperature::getMemoryTemp(double *memTemp)
{
	TRACING();
	ze_result_t result = getTemp(ZES_TEMP_SENSORS_MEMORY, memTemp);
	// LPDDR5 reports an MR4 thermal code (0..7); convert to Celsius.
	if (result == ZE_RESULT_SUCCESS && hasLpddr5Memory && memTemp != nullptr) {
		*memTemp = mr4CodeToCelsius(*memTemp);
	}
	return result;
}

/**
 * @brief Gets GPU core temperature threshold values
 *
 * This function retrieves the thermal threshold values for GPU core components,
 * including throttle and shutdown temperature limits used for thermal protection
 * and performance management.
 *
 * @param device Handle to the device
 * @param throttleThreshold Pointer to store the throttle temperature threshold
 * @param shutdownThreshold Pointer to store the shutdown temperature threshold
 * @return ze_result_t ZE_RESULT_SUCCESS on successful threshold retrieval, error code otherwise
 */
ze_result_t temperature::getCoreThreshold(zes_device_handle_t device, uint32_t *throttleThreshold,
										  uint32_t *shutdownThreshold)
{
	TRACING();

	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_device_properties_t props = {};
	props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
	props.pNext = nullptr;
	if (!(zesDeviceGetProperties(device, &props) == ZE_RESULT_SUCCESS)) {
		ERR("Failed to get device ID\n");
		return result;
	}
	*throttleThreshold = (uint32_t)getThrottleCoreTemperature(props.core.deviceId);
	*shutdownThreshold = (uint32_t)getShutdownCoreTemperature(props.core.deviceId);
	return result;
}

/**
 * @brief Gets memory temperature threshold values
 *
 * This function retrieves the thermal threshold values for memory components,
 * including throttle and shutdown temperature limits used for memory thermal
 * protection and system stability management.
 *
 * @param device Handle to the device
 * @param throttleThreshold Pointer to store the memory throttle temperature threshold
 * @param shutdownThreshold Pointer to store the memory shutdown temperature threshold
 * @return ze_result_t ZE_RESULT_SUCCESS on successful threshold retrieval, error code otherwise
 */
ze_result_t temperature::getMemoryThreshold(zes_device_handle_t device, uint32_t *throttleThreshold,
											uint32_t *shutdownThreshold)
{
	TRACING();

	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_device_properties_t props = {};
	props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
	props.pNext = nullptr;
	if (!(zesDeviceGetProperties(device, &props) == ZE_RESULT_SUCCESS)) {
		ERR("Failed to get device ID\n");
		return result;
	}
	*throttleThreshold = MEMORY_THROTTLE_THRESHOLD_DEFAULT;
	*shutdownThreshold = (uint32_t)getShutdownMemoryTemperature(props.core.deviceId);

	return result;
}

/**
 * @brief Initializes the temperature management module for a device
 *
 * This function performs initial setup of temperature monitoring capabilities
 * by enumerating all available temperature sensors on the specified device
 * for subsequent thermal monitoring operations.
 *
 * @param device Handle to the device for temperature initialization
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t temperature::init(zes_device_handle_t device)
{
	TRACING();
	ze_result_t result = enumTemperatureDomains(device);
	// Detect LPDDR5 memory so memory-temp getters can convert the MR4 thermal
	// code reported by the device into a Celsius value. Best-effort: the result
	// is intentionally ignored so a detection failure does not mask the
	// enumeration result; on failure the LPDDR5 flag stays false and behavior
	// is unchanged for non-LPDDR5 devices.
	(void)detectLpddr5Memory(device);
	return result;
}

/**
 * @brief Performs comprehensive temperature monitoring runtime operations
 *
 * This function executes a complete temperature monitoring cycle for all
 * temperature sensors, including property retrieval and current temperature
 * readings for comprehensive thermal analysis and system health monitoring.
 *
 * @param device Handle to the device (unused in current implementation)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful execution, error code otherwise
 */
ze_result_t temperature::zesRun(UNUSED zes_device_handle_t device)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_temp_properties_t properties;
	double temp;

	for (uint32_t i = 0; i < temperatureCount; ++i) {
		result = getProperties(temperatureHandles[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = getState(temperatureHandles[i], &temp);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
	}

	return result;
}
/**
 * @brief Convert a JEDEC LPDDR5 MR4 thermal refresh code to Celsius.
 *
 * LPDDR5 devices expose temperature through Mode Register 4 (MR4) OP[2:0] as
 * a 3-bit refresh-rate code (0..7) that maps to a temperature range rather
 * than a single Celsius value. Per the JIRA, the agreed-upon convention for
 * pcode and the software stack is to report the maximum temperature of the
 * range associated with each code. This table follows the standard JEDEC
 * JESD209-5 LPDDR5 MR4 thermal mapping.
 *
 *  MR4  Refresh Rate     Range (°C)        Reported (max of range)
 *  ---  ---------------  ----------------  -----------------------
 *   0   reserved/low     T < low limit                       0
 *   1   4x               T <= 45                            45
 *   2   2x               45 < T <= 85                       85
 *   3   1x (nominal)     85 < T <= 95                       95
 *   4   0.5x             95 < T <= 105                     105
 *   5   0.25x           105 < T <= 110                     110
 *   6   0.25x + derate  110 < T <= 125                     125
 *   7   reserved/high    T > high limit                    125
 *
 * Values outside [0, LPDDR5_MR4_MAX_CODE] are returned unchanged so that
 * callers passing an already-converted Celsius value are not double-mapped.
 *
 * @param rawValue The raw value returned by zesTemperatureGetState for an
 *                 LPDDR5 memory sensor.
 * @return Maximum temperature (Celsius) of the MR4 range, or rawValue if the
 *         input is not a recognized MR4 code.
 */
double temperature::mr4CodeToCelsius(double rawValue)
{
	static const double kMr4ToCelsius[LPDDR5_MR4_MAX_CODE + 1] = {
		0.0,   // 0: low temperature operating limit exceeded
		45.0,  // 1: 4x refresh
		85.0,  // 2: 2x refresh
		95.0,  // 3: 1x refresh (nominal)
		105.0, // 4: 0.5x refresh
		110.0, // 5: 0.25x refresh
		125.0, // 6: 0.25x refresh with derating
		125.0, // 7: high temperature operating limit exceeded
	};

	// Only convert values that look like an MR4 code (integer 0..7).
	if (!std::isfinite(rawValue) || rawValue < 0.0 || rawValue > (double)LPDDR5_MR4_MAX_CODE) {
		return rawValue;
	}
	double rounded = std::round(rawValue);
	if (std::fabs(rawValue - rounded) > 1e-9) {
		return rawValue;
	}
	uint32_t code = (uint32_t)rounded;
	return kMr4ToCelsius[code];
}

/**
 * @brief Detect whether any memory module on the device is LPDDR5.
 *
 * When LPDDR5 memory is present, memory temperature readings arrive as MR4
 * thermal codes rather than Celsius values and require conversion via
 * mr4CodeToCelsius().
 *
 * @param device Sysman device handle
 * @return ZE_RESULT_SUCCESS on successful enumeration; the LPDDR5 flag is
 *         cleared on any failure so behavior remains unchanged for
 *         non-LPDDR5 devices.
 */
ze_result_t temperature::detectLpddr5Memory(zes_device_handle_t device)
{
	hasLpddr5Memory = false;

	uint32_t count = 0;
	ze_result_t result = zesDeviceEnumMemoryModules(device, &count, nullptr);
	if (result != ZE_RESULT_SUCCESS || count == 0) {
		DBG("No memory modules to inspect for LPDDR5 (0x{:X})\n", result);
		return result;
	}

	std::vector<zes_mem_handle_t> handles(count);
	result = zesDeviceEnumMemoryModules(device, &count, handles.data());
	if (result != ZE_RESULT_SUCCESS) {
		DBG("Failed to retrieve memory module handles (0x{:X})\n", result);
		return result;
	}

	for (uint32_t i = 0; i < count; ++i) {
		zes_mem_properties_t props = {};
		props.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
		if (zesMemoryGetProperties(handles[i], &props) == ZE_RESULT_SUCCESS && props.type == ZES_MEM_TYPE_LPDDR5) {
			hasLpddr5Memory = true;
			DBG("LPDDR5 memory detected; MR4 -> Celsius conversion enabled\n");
			break;
		}
	}

	return ZE_RESULT_SUCCESS;
}
