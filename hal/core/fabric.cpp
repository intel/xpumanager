/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "fabric.h"

/**
 * @brief Destructor for the fabric class
 *
 * This destructor performs cleanup operations for the fabric management
 * object, releasing allocated memory for fabric port handles and ensuring
 * proper resource deallocation when the fabric object is destroyed.
 */
fabric::~fabric()
{
	if (ports) {
		delete[] ports;
		ports = nullptr;
	}
}

/**
 * @brief Enumerates available fabric ports for a device
 *
 * This function discovers and catalogs all fabric ports available on the
 * specified device. Fabric ports represent high-speed interconnect interfaces
 * such as Xe Link ports for device-to-device communication.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
ze_result_t fabric::enumFabricPorts(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumFabricPorts(device, &portCount, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to enumerate fabric ports: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}
	DBG("Device has {} fabric ports\n", portCount);

	if (portCount == 0)
		return ZE_RESULT_SUCCESS;

	ports = new zes_fabric_port_handle_t[portCount];
	result = zesDeviceEnumFabricPorts(device, &portCount, ports);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get fabric port handles: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	return result;
}

/**
 * @brief Gets properties for a specific fabric port
 *
 * This function retrieves detailed properties and characteristics for a
 * specific fabric port, including port identification, speed capabilities,
 * and physical connection information.
 *
 * @param hFabricPort Handle to the specific fabric port
 * @param properties Caller-allocated buffer to receive properties
 * @return ze_result_t ZE_RESULT_SUCCESS on successful property retrieval, error code otherwise
 */
ze_result_t fabric::portGetProperties(zes_fabric_port_handle_t hFabricPort, zes_fabric_port_properties_t *properties)
{
	ze_result_t result = zesFabricPortGetProperties(hFabricPort, properties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get fabric port properties: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Fabric Ports Properties:\n");
	DBG("  - Fabric Id: {}\n", properties->portId.fabricId);
	DBG("  - Attach Id: {}\n", properties->portId.attachId);
	DBG("  - Port Number: {}\n", properties->portId.portNumber);
	DBG("  - Port SType: {}\n", properties->stype);
	DBG("  - Port model: {}\n", properties->model);
	DBG("  - Port OnSubdevice ID: {}\n", properties->onSubdevice);
	DBG("  - Port Subdevice ID: {}\n", properties->subdeviceId);
	DBG("  - Port maxRxSpeed bitRate: {}\n", (unsigned long long)properties->maxRxSpeed.bitRate);
	DBG("  - Port maxRxSpeed width: {}\n", properties->maxRxSpeed.width);
	DBG("  - Port maxTxSpeed bitRate: {}\n", (unsigned long long)properties->maxTxSpeed.bitRate);
	DBG("  - Port maxTxSpeed width: {}\n", properties->maxTxSpeed.width);

	return result;
}

/**
 * @brief Gets comprehensive information for all fabric ports on a device
 *
 * This function enumerates all fabric ports on the specified device and collects
 * detailed information including properties, state, link type, and configuration
 * for each port. The collected data is returned in a vector for easy processing
 * and analysis of fabric interconnect topology.
 *
 * @param [in] device Handle to the Level Zero Sysman device
 * @param [out] portInfoIn Vector to receive port information structures, each containing
 *                      properties, state, link type, and configuration for one port
 * @retval ZE_RESULT_SUCCESS Fabric port information retrieved successfully
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE No fabric ports found on device
 * @retval ZE_RESULT_ERROR_INVALID_NULL_HANDLE Device handle is nullptr
 * @retval Other error codes from zesDeviceEnumFabricPorts, zesFabricPortGetProperties,
 *         zesFabricPortGetState, zesFabricPortGetLinkType, or zesFabricPortGetConfig
 */
ze_result_t fabric::getFabricPorts(zes_device_handle_t device, std::vector<portInfo> &portInfoIn)
{
	if (device == nullptr) {
		ERR("Device handle is null\n");
		return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
	}

	ze_result_t res;

	// If ports not already enumerated, enumerate them
	if (ports == nullptr) {
		res = enumFabricPorts(device);
		if (res != ZE_RESULT_SUCCESS) {
			return res;
		}
	}

	if (portCount == 0 || ports == nullptr) {
		DBG("No fabric ports found\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	// Use the cached port handles
	for (uint32_t i = 0; i < portCount; i++) {
		portInfo info = {};

		// Get properties
		info.portProps.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES;
		res = zesFabricPortGetProperties(ports[i], &info.portProps);
		if (res != ZE_RESULT_SUCCESS) {
			ERR("Failed to get fabric port properties: 0x{:X} ({})\n", res, l0_error_to_string(res));
			continue;
		}

		// Get state
		info.portState.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_STATE;
		res = zesFabricPortGetState(ports[i], &info.portState);
		if (res != ZE_RESULT_SUCCESS) {
			DBG("Failed to get fabric port state: 0x{:X} ({}) port:{}.{}.{}\n", res, l0_error_to_string(res),
				info.portProps.portId.fabricId, info.portProps.portId.attachId, info.portProps.portId.portNumber);
		}

		// Get link type
		res = zesFabricPortGetLinkType(ports[i], &info.portLink);
		if (res != ZE_RESULT_SUCCESS) {
			DBG("Failed to get fabric port link type: 0x{:X} ({}) port:{}.{}.{}\n", res, l0_error_to_string(res),
				info.portProps.portId.fabricId, info.portProps.portId.attachId, info.portProps.portId.portNumber);
		}

		// Get configuration
		info.portConf.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_CONFIG;
		res = zesFabricPortGetConfig(ports[i], &info.portConf);
		if (res != ZE_RESULT_SUCCESS) {
			DBG("Failed to get fabric port config: 0x{:X} ({}) port:{}.{}.{}\n", res, l0_error_to_string(res),
				info.portProps.portId.fabricId, info.portProps.portId.attachId, info.portProps.portId.portNumber);
		}

		portInfoIn.push_back(info);
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Sets configuration for a specific fabric port
 *
 * This function modifies the configuration of a specific fabric port identified
 * by subdevice ID and port ID. It allows selective configuration of port enabled
 * state and beaconing feature based on the flags set in the portInfoSet structure.
 *
 * @param [in] device Handle to the Level Zero Sysman device
 * @param [in] portInfoSetIn Configuration structure containing subdeviceId, portId,
 *                        and configuration flags (enabled, beaconing) along with
 *                        settingEnabled and settingBeaconing to control which
 *                        fields should be modified
 * @retval ZE_RESULT_SUCCESS Fabric port configuration updated successfully
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE No fabric ports found on device
 * @retval ZE_RESULT_ERROR_INVALID_NULL_HANDLE Device handle is nullptr
 * @retval ZE_RESULT_ERROR_INVALID_ARGUMENT Specified fabric port not found
 * @retval Other error codes from zesDeviceEnumFabricPorts, zesFabricPortGetProperties,
 *         zesFabricPortGetConfig, or zesFabricPortSetConfig
 */
ze_result_t fabric::setFabricPorts(zes_device_handle_t device, const portInfoSet &portInfoSetIn)
{
	if (device == nullptr) {
		ERR("Device handle is null\n");
		return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
	}

	ze_result_t res;

	// If ports not already enumerated, enumerate them
	if (ports == nullptr) {
		res = enumFabricPorts(device);
		if (res != ZE_RESULT_SUCCESS) {
			return res;
		}
	}

	if (portCount == 0 || ports == nullptr) {
		ERR("No fabric ports found\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	// Use the cached port handles
	bool found = false;
	for (uint32_t i = 0; i < portCount; i++) {
		zes_fabric_port_properties_t props = {};
		zes_fabric_port_config_t config = {};

		props.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES;
		res = zesFabricPortGetProperties(ports[i], &props);
		if (res != ZE_RESULT_SUCCESS) {
			ERR("Failed to get fabric port properties: 0x{:X} ({})\n", res, l0_error_to_string(res));
			continue;
		}

		// Match the port based on subdeviceId and portId
		if (props.subdeviceId != portInfoSetIn.subdeviceId ||
			props.portId.portNumber != portInfoSetIn.portId.portNumber) {
			continue;
		}

		found = true;

		// Get current configuration
		config.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_CONFIG;
		res = zesFabricPortGetConfig(ports[i], &config);
		if (res != ZE_RESULT_SUCCESS) {
			ERR("Failed to get current fabric port config: 0x{:X} ({})\n", res, l0_error_to_string(res));
			return res;
		}

		// Update only the fields that are set
		if (portInfoSetIn.settingEnabled) {
			config.enabled = portInfoSetIn.enabled;
		}
		if (portInfoSetIn.settingBeaconing) {
			config.beaconing = portInfoSetIn.beaconing;
		}

		// Set new configuration
		res = zesFabricPortSetConfig(ports[i], &config);
		if (res != ZE_RESULT_SUCCESS) {
			ERR("Failed to set fabric port config: 0x{:X} ({})\n", res, l0_error_to_string(res));
			return res;
		}

		break;
	}

	if (!found) {
		ERR("Fabric port not found: subdevice {}, port {}\n", portInfoSetIn.subdeviceId,
			portInfoSetIn.portId.portNumber);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Gets the link type for a specific fabric port
 *
 * This function retrieves the link type information for a fabric port,
 * which describes the physical connection technology and protocol used
 * by the high-speed interconnect interface.
 *
 * @param hFabricPort Handle to the specific fabric port
 * @return ze_result_t ZE_RESULT_SUCCESS on successful link type retrieval, error code otherwise
 */
ze_result_t fabric::portGetLinkType(zes_fabric_port_handle_t hFabricPort)
{
	zes_fabric_link_type_t linkType;
	// Get the link type of the fabric port
	ze_result_t result = zesFabricPortGetLinkType(hFabricPort, &linkType);
	if (result == ZE_RESULT_SUCCESS) {
		DBG("Fabric Port Link Type: {}\n", linkType.desc);
	} else {
		ERR("Error getting fabric port link type: 0x{:X} ({})\n", result, l0_error_to_string(result));
	}
	return result;
}

/**
 * @brief Gets the current configuration for a specific fabric port
 *
 * This function retrieves the current configuration settings for a fabric port,
 * including whether the port is enabled and beaconing status for port identification.
 *
 * @param hFabricPort Handle to the specific fabric port
 * @return ze_result_t ZE_RESULT_SUCCESS on successful configuration retrieval, error code otherwise
 */
ze_result_t fabric::portGetConfig(zes_fabric_port_handle_t hFabricPort)
{
	zes_fabric_port_config_t portConfig = {};
	ze_result_t result = zesFabricPortGetConfig(hFabricPort, &portConfig);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get fabric port configuration: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Fabric Port Configuration:");
	DBG("  - Enabled: {}\n", (portConfig.enabled ? "Yes" : "No"));
	DBG("  - Beaconing: {}\n", (portConfig.beaconing ? "Enabled" : "Disabled"));

	return result;
}

/**
 * @brief Gets the current state for a specific fabric port
 *
 * This function retrieves the current operational state of a fabric port,
 * including connection status, quality issues, failure reasons, and current
 * transmission speeds for both receive and transmit directions.
 *
 * @param hFabricPort Handle to the specific fabric port
 * @param state Caller-allocated buffer to receive state
 * @return ze_result_t ZE_RESULT_SUCCESS on successful state retrieval, error code otherwise
 */
ze_result_t fabric::portGetState(zes_fabric_port_handle_t hFabricPort, zes_fabric_port_state_t *state)
{
	ze_result_t result = zesFabricPortGetState(hFabricPort, state);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get fabric port state: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Fabric Port State:");
	DBG("  - Status: {}\n", state->status);
	DBG("  - Quality Issues: {}\n", state->qualityIssues);
	DBG("  - Failure Reasons: {}\n", state->failureReasons);
	DBG("  - Remote Port Id - Fabric Id: {}\n", state->remotePortId.fabricId);
	DBG("  - Remote Port Id - Attach Id: {}\n", state->remotePortId.attachId);
	DBG("  - Remote Port Id - Port Number: {}\n", state->remotePortId.portNumber);
	DBG("  - Rx Speed - Bit Rate: {}\n", (unsigned long long)state->rxSpeed.bitRate);
	DBG("  - Rx Speed - Width: {}\n", state->rxSpeed.width);
	DBG("  - Tx Speed - Bit Rate: {}\n", (unsigned long long)state->txSpeed.bitRate);
	DBG("  - Tx Speed - Width: {}\n", state->txSpeed.width);

	return result;
}

/**
 * @brief Gets throughput statistics for a specific fabric port
 *
 * This function retrieves current throughput metrics for a fabric port,
 * including receive and transmit byte counters along with timestamp
 * information for bandwidth calculations.
 *
 * @param hFabricPort Handle to the specific fabric port
 * @param throughput Caller-allocated buffer to receive throughput
 * @return ze_result_t ZE_RESULT_SUCCESS on successful throughput retrieval, error code otherwise
 */
ze_result_t fabric::portGetThroughput(zes_fabric_port_handle_t hFabricPort, zes_fabric_port_throughput_t *throughput)
{
	ze_result_t result = zesFabricPortGetThroughput(hFabricPort, throughput);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get fabric port throughput: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Fabric Port Throughput:");
	DBG("  - Rx Counter: {}\n", throughput->rxCounter);
	DBG("  - Tx Counter: {}\n", throughput->txCounter);
	DBG("  - Timestamp: {}\n", throughput->timestamp);

	return result;
}

/**
 * @brief Gets error counters for a specific fabric port
 *
 * This function retrieves various error statistics for a fabric port,
 * including link failure counts, degradation events, and firmware-related
 * communication errors for reliability monitoring.
 *
 * @param hFabricPort Handle to the specific fabric port
 * @return ze_result_t ZE_RESULT_SUCCESS on successful error counter retrieval, error code otherwise
 */
ze_result_t fabric::portGetFabricErrorCounters(zes_fabric_port_handle_t hFabricPort)
{
	zes_fabric_port_error_counters_t errorCounters = {};
	ze_result_t result = zesFabricPortGetFabricErrorCounters(hFabricPort, &errorCounters);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get fabric port error counters: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Fabric Port Error Counters:");
	DBG("  - Link Failure Count: {}\n", errorCounters.linkFailureCount);
	DBG("  - Link Degrade Count: {}\n", errorCounters.linkDegradeCount);
	DBG("  - Firmware reported Error Count: {}\n", errorCounters.fwErrorCount);
	DBG("  - Firmware Communication Error Count: {}\n", errorCounters.fwCommErrorCount);

	return result;
}

/**
 * @brief Gets throughput statistics for multiple fabric ports simultaneously
 *
 * This function efficiently retrieves throughput metrics for multiple fabric
 * ports in a single operation, providing synchronized bandwidth measurements
 * across all specified ports for system-wide performance analysis.
 *
 * @param device Handle to the Level Zero Sysman device
 * @param count Number of fabric ports to query
 * @param throughputs Caller-allocated buffer array to receive throughputs
 * @return ze_result_t ZE_RESULT_SUCCESS on successful multi-port throughput retrieval, error code otherwise
 */
ze_result_t fabric::portGetMultiPortThroughput(zes_device_handle_t device, uint32_t count,
											   zes_fabric_port_throughput_t *throughputs)
{
	if (count == 0 || device == nullptr) {
		ERR("Invalid fabric port handles or count.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	ze_result_t result = zesFabricPortGetMultiPortThroughput(device, count, ports, &throughputs);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get multi-port throughput: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Multi-Port Throughput:");
	for (uint32_t i = 0; i < count; i++) {
		DBG("Port {}:\n", i);
		DBG("  - Rx Counter: {}\n", throughputs[i].rxCounter);
		DBG("  - Tx Counter: {}\n", throughputs[i].txCounter);
		DBG("  - Timestamp: {}\n", throughputs[i].timestamp);
	}

	return result;
}

/**
 * @brief Gets the handle for a specific fabric port by index
 *
 * This function retrieves the fabric port handle at the specified index
 * from the enumerated list of fabric ports.
 *
 * @param index Zero-based index of the fabric port
 * @return zes_fabric_port_handle_t Handle to the fabric port, or nullptr if index is invalid
 */
zes_fabric_port_handle_t fabric::getPortHandle(uint32_t index) const
{
	if (ports == nullptr || index >= portCount) {
		return nullptr;
	}
	return ports[index];
}

/**
 * @brief Sets the configuration for all fabric ports
 *
 * This function configures the enabled/disabled state for all fabric ports
 * on the device, allowing system-wide control of high-speed interconnect
 * interfaces for power management or maintenance operations.
 *
 * @param enabled Boolean flag to enable (true) or disable (false) all fabric ports
 * @return ze_result_t ZE_RESULT_SUCCESS if all ports configured successfully, error code otherwise
 */
ze_result_t fabric::setPortConfig(bool enabled)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_fabric_port_config_t config;

	for (uint32_t i = 0; i < portCount; i++) {
		result = zesFabricPortGetConfig(ports[i], &config);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get fabric port configuration: 0x{:X} ({})\n", result, l0_error_to_string(result));
			return result;
		}

		config.enabled = enabled;

		result = zesFabricPortSetConfig(ports[i], &config);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to set fabric port configuration: 0x{:X} ({})\n", result, l0_error_to_string(result));
			return result;
		}
		DBG("Fabric Port {} configuration set to {}\n", i, (enabled ? "Enabled" : "Disabled"));
	}
	return result;
}

/**
 * @brief Sets the beaconing configuration for all fabric ports
 *
 * This function configures the beaconing feature for all fabric ports,
 * which enables or disables port identification signals useful for
 * physical port location and troubleshooting in multi-device systems.
 *
 * @param enabled Boolean flag to enable (true) or disable (false) beaconing on all fabric ports
 * @return ze_result_t ZE_RESULT_SUCCESS if beaconing configured successfully on all ports, error code otherwise
 */
ze_result_t fabric::setPortBeaconing(bool enabled)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_fabric_port_config_t config;

	for (uint32_t i = 0; i < portCount; i++) {
		result = zesFabricPortGetConfig(ports[i], &config);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to get fabric port configuration: 0x{:X} ({})\n", result, l0_error_to_string(result));
			return result;
		}

		config.beaconing = enabled;

		result = zesFabricPortSetConfig(ports[i], &config);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to set fabric port beaconing: 0x{:X} ({})\n", result, l0_error_to_string(result));
			return result;
		}
		DBG("Fabric Port {} beaconing set to {}\n", i, (enabled ? "Enabled" : "Disabled"));
	}
	return result;
}

/**
 * @brief Initializes the fabric management subsystem for a device
 *
 * This function initializes fabric port management by enumerating all
 * available fabric ports on the device and preparing them for monitoring
 * and configuration operations.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t fabric::init(zes_device_handle_t device) { return enumFabricPorts(device); }

/**
 * @brief Executes comprehensive fabric port monitoring and data collection
 *
 * This function performs a complete fabric port assessment by collecting
 * properties, configuration, state, throughput, and error statistics for
 * all fabric ports, providing comprehensive interconnect health monitoring.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS if all fabric operations completed successfully, error code otherwise
 */
ze_result_t fabric::zesRun(zes_device_handle_t device)
{
	ze_result_t result = ZE_RESULT_SUCCESS;

	for (uint32_t i = 0; i < portCount; i++) {
		zes_fabric_port_properties_t properties = {};
		result = portGetProperties(ports[i], &properties);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = portGetLinkType(ports[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = portGetConfig(ports[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		zes_fabric_port_state_t state = {};
		result = portGetState(ports[i], &state);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		zes_fabric_port_throughput_t throughput = {};
		result = portGetThroughput(ports[i], &throughput);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = portGetFabricErrorCounters(ports[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
	}

	if (portCount > 0) {
		std::vector<zes_fabric_port_throughput_t> throughputs(portCount);
		result = portGetMultiPortThroughput(device, portCount, throughputs.data());
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
	}

	return result;
}