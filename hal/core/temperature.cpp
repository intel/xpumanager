/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "temperature.h"
#include "debug.h"
#include <map>
#include <memory>

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
				ERR("Error parsing %s threshold for device %s: %s\n", key.c_str(), it.key().c_str(), e.what());
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
		ERR("Error loading temperature thresholds config: %s\n", e.what());
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
	  defaultMemoryShutdownThreshold(MEMORY_SHUTDOWN_THRESHOLD_DEFAULT)
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
		ERR("Failed to enumerate temperature domains. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	temperatureHandles = new zes_temp_handle_t[temperatureCount];
	result = zesDeviceEnumTemperatureSensors(device, &temperatureCount, temperatureHandles);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get temperature domains. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Found %u temperature domains.\n", temperatureCount);
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
		ERR("Failed to get properties for temperature domain 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Temperature Properties:\n");
	DBG("  onSubdevice: %d\n", properties->onSubdevice);
	DBG("  subdeviceId: %u\n", properties->subdeviceId);
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
		DBG("  type: Unknown (%d)\n", properties->type);
		break;
	}
	DBG("  maxTemperature: %f C\n", properties->maxTemperature);
	DBG("  isCriticalTempSupported: %d\n", properties->isCriticalTempSupported);
	DBG("  isThreshold1Supported: %d\n", properties->isThreshold1Supported);
	DBG("  isThreshold2Supported: %d\n", properties->isThreshold2Supported);

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
		DBG("Failed to get state for temperature domain 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Temperature State:\n");
	DBG("  Current Temperature: %f C\n", *temp);

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
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_temp_properties_t properties;
	*temp = 0.0; // Default value if no temperature found
	bool sensorFound = false;

	for (uint32_t i = 0; i < temperatureCount; ++i) {
		result = getProperties(temperatureHandles[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			DBG("Failed to get properties for temperature sensor %u: 0x%X (%s)\n", i, result,
				l0_error_to_string(result));
			continue;
		}

		if (properties.type == type) {
			sensorFound = true;
			result = getState(temperatureHandles[i], temp);
			if (result == ZE_RESULT_SUCCESS) {
				return result;
			} else {
				DBG("Failed to get state for temperature sensor %u (type %d): 0x%X (%s)\n", i, type, result,
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
			DBG("Failed to get properties for temperature sensor %u\n", i);
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
			DBG("Failed to get state for temperature sensor %u: 0x%X (%s)\n", i, result, l0_error_to_string(result));
			continue;
		}

		// Filter abnormal temperatures
		if (temp < MAX_REASONABLE_TEMP_CELSIUS) {
			tileTemperatures[tileId] = temp;
			DBG("Tile %u %s temperature: %.2f C\n", tileId,
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
	return getTemp(ZES_TEMP_SENSORS_MEMORY, memTemp);
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
	return enumTemperatureDomains(device);
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