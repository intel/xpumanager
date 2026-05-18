/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "pci.h"
#include <assert.h>
#include <gscupd.h>
#include <string>
#include <vector>

// Suppress false positive warnings from GCC's static analysis of std::regex implementation
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include <regex>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

/**
 * @brief Gets PCI properties for a device
 *
 * This function retrieves comprehensive PCI configuration information for the
 * specified device, including PCI address, speed capabilities, bandwidth limits,
 * and generation information for system topology analysis.
 *
 * @param device Handle to the device
 * @param pciProps Pointer to structure to store PCI properties
 * @return ze_result_t ZE_RESULT_SUCCESS on successful property retrieval, error code otherwise
 */
ze_result_t pci::getProperties(zes_device_handle_t device, zes_pci_properties_t *pciProps)
{
	assert(pciProps);
	if (!pciProps) {
		ERR("Invalid argument: pciProps is null\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	memset(pciProps, 0, sizeof(zes_pci_properties_t));

	ze_result_t result = zesDevicePciGetProperties(device, pciProps);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI properties: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("  - PCI Properties:\n");
	DBG("    - Address: {}:{}:{}.{}\n", pciProps->address.domain, pciProps->address.bus, pciProps->address.device,
		pciProps->address.function);
	DBG("    - Max Speed: {} Gen, {} lanes, Max bandwidth: {}\n", pciProps->maxSpeed.gen, pciProps->maxSpeed.width,
		(unsigned long long)pciProps->maxSpeed.maxBandwidth);

	DBG("    - haveBandwidthCounters: {}\n", pciProps->haveBandwidthCounters);
	DBG("    - havePacketCounters: {}\n", pciProps->havePacketCounters);
	DBG("    - haveReplayCounters: {}\n", pciProps->haveReplayCounters);
	return result;
}

/**
 * @brief Gets the current PCI state for a device
 *
 * This function retrieves the current PCI state information for the specified
 * device, including current speed, width, and bandwidth utilization for
 * real-time PCI performance monitoring.
 *
 * @param device Handle to the device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful state retrieval, error code otherwise
 */
ze_result_t pci::getState(zes_device_handle_t device, zes_pci_link_status_t &pciLinkStatus)
{
	zes_pci_state_t pciState = {};
	ze_result_t result = zesDevicePciGetState(device, &pciState);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI state: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	pciLinkStatus = pciState.status;

	DBG("  - PCI State:");
	DBG("    - Status: {}\n", pciState.status);
	DBG("    - Stability Issues: {}\n", pciState.stabilityIssues);
	DBG("    - Quality Issues: {}\n", pciState.qualityIssues);
	DBG("    - Current Speed: {} Gen, {} lanes, {}Max bandwidth\n", pciState.speed.gen, pciState.speed.width,
		(unsigned long long)pciState.speed.maxBandwidth);
	return result;
}

/**
 * @brief Gets the current negotiated PCIe link speed for a device
 *
 * Retrieves the current PCIe state and returns the speed (generation and
 * width) of the negotiated link.
 *
 * @param device Handle to the device
 * @param speed Output structure filled with the current link speed
 * @return ze_result_t ZE_RESULT_SUCCESS on success, error code otherwise
 */
ze_result_t pci::getCurrentLinkSpeed(zes_device_handle_t device, zes_pci_speed_t &speed)
{
	zes_pci_state_t state{};
	ze_result_t result = zesDevicePciGetState(device, &state);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI state for link speed: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}
	speed = state.speed;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Gets PCI Base Address Registers (BARs) information for a device
 *
 * This function retrieves PCI BAR information for the specified device,
 * including memory-mapped I/O regions and their sizes for low-level
 * hardware access and memory mapping analysis.
 *
 * @param device Handle to the device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful BAR retrieval, error code otherwise
 */
ze_result_t pci::getBars(zes_device_handle_t device)
{
	// This function is currently a placeholder.
	// Get PCI BARs of each device
	uint32_t barCount = 0;
	ze_result_t result = zesDevicePciGetBars(device, &barCount, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI BAR count: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	std::vector<zes_pci_bar_properties_t> bars(barCount);
	result = zesDevicePciGetBars(device, &barCount, bars.data());
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI BAR properties: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("  - PCI BARs:\n");
	for (const auto &bar : bars) {
		DBG("    - Index: {}\n", bar.index);
		DBG("    - Base Address: 0x{:x}\n", bar.base);
		DBG("    - Size: 0x{:x}bytes\n", bar.size);
		DBG("    - Type: ");
		if (bar.type == ZES_PCI_BAR_TYPE_MMIO) {
			DBG("MMIO");
		} else if (bar.type == ZES_PCI_BAR_TYPE_ROM) {
			DBG("ROM");
		} else if (bar.type == ZES_PCI_BAR_TYPE_MEM) {
			DBG("MEM");
		} else {
			DBG("Other");
		}
		DBG("\n");
	}
	return result;
}

/**
 * @brief Gets PCI statistics for a device
 *
 * This function retrieves comprehensive PCI performance statistics for the
 * specified device, including bandwidth utilization, packet counts, and
 * replay counters for detailed PCI performance analysis.
 *
 * @param device Handle to the device
 * @param pciStats Pointer to structure to store PCI statistics
 * @return ze_result_t ZE_RESULT_SUCCESS on successful statistics retrieval, error code otherwise
 */
ze_result_t pci::getStats(zes_device_handle_t device, zes_pci_stats_t *pciStats)
{
	TRACING();

	if (!pciStats) {
		ERR("Invalid argument: pciStats is null\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}
	memset(pciStats, 0, sizeof(zes_pci_stats_t));

	ze_result_t result = zesDevicePciGetStats(device, pciStats);
	if (result != ZE_RESULT_SUCCESS) {
		DBG("Failed to get PCI stats: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("  - PCI Stats:");
	DBG("    - Timestamp: {}\n", pciStats->timestamp);
	DBG("    - Replay counter: {}\n", pciStats->replayCounter);
	DBG("    - Packet counter: {}\n", pciStats->packetCounter);
	DBG("    - Received Bytes: {}\n", pciStats->rxCounter);
	DBG("    - Transmitted Bytes: {}\n", pciStats->txCounter);

	return result;
}

/**
 * @brief Gets the PCIe downgrade state for a device
 *
 * This function retrieves the PCIe generation downgrade state information for the
 * specified device, including whether downgrade is supported, currently enabled,
 * and any pending actions required for the configuration to take effect.
 *
 * @param[in] device Handle to the device
 * @param[out] state Reference to PciDowngradeState structure to store the downgrade state
 * @return ze_result_t ZE_RESULT_SUCCESS on successful state retrieval, error code otherwise
 */
ze_result_t pci::getPciDowngradeState(const zes_device_handle_t &device, PciDowngradeState &state)
{
	TRACING();
	ze_result_t res;
	zes_pci_properties_t pciProps = {};
	zes_pci_link_speed_downgrade_ext_properties_t downProps = {};

	downProps.stype = ZES_STRUCTURE_TYPE_PCI_LINK_SPEED_DOWNGRADE_EXT_PROPERTIES;
	pciProps.pNext = &downProps;
	res = zesDevicePciGetProperties(device, &pciProps);
	if (res != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI properties: 0x{:X} ({})\n", res, l0_error_to_string(res));
		return res;
	}

	state.downgradeSupported = downProps.pciLinkSpeedUpdateCapable;
	if (!state.downgradeSupported) {
		DBG("PCIe Link Speed Downgrade is not supported\n");
		return ZE_RESULT_SUCCESS;
	}

	zes_pci_state_t pciState = {};
	zes_pci_link_speed_downgrade_ext_state_t downState = {};

	downState.stype = ZES_STRUCTURE_TYPE_PCI_LINK_SPEED_DOWNGRADE_EXT_STATE;
	pciState.pNext = &downState;
	res = zesDevicePciGetState(device, &pciState);
	if (res != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI State: 0x{:X} ({})\n", res, l0_error_to_string(res));
		return res;
	}

	state.downgradeEnabled = downState.pciLinkSpeedDowngradeStatus;

	DBG("  - PCIe Downgrade State:\n");
	DBG("    - Downgrade Supported: {}\n", state.downgradeSupported ? "Yes" : "No");
	DBG("    - Downgrade Enabled: {}\n", state.downgradeEnabled ? "Yes" : "No");
	DBG("    - Pending Action: {}\n", state.pendingAction);

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Sets the PCIe downgrade state for a device
 *
 * This function enables or disables PCIe generation downgrade for the specified device.
 * PCIe downgrade allows the device to operate at a lower PCIe generation for compatibility
 * or power management purposes. A device reset may be required for changes to take effect.
 *
 * @param[in] device Handle to the device
 * @param[in] enabled Boolean flag to enable (true) or disable (false) PCIe downgrade
 * @param[out] state Reference to PciDowngradeState structure to store the updated downgrade state
 * @return ze_result_t ZE_RESULT_SUCCESS on successful state update, error code otherwise
 */
ze_result_t pci::setPciDowngradeState(const zes_device_handle_t &device, bool enabled, PciDowngradeState &state)
{
	TRACING();

	// First check if PCIe downgrade is supported
	PciDowngradeState currentState = {};
	ze_result_t res = getPciDowngradeState(device, currentState);
	if (res != ZE_RESULT_SUCCESS) {
		ERR("Failed to get current PCIe downgrade state: 0x{:X} ({})\n", res, l0_error_to_string(res));
		return res;
	}

	if (!currentState.downgradeSupported) {
		ERR("PCIe Link Speed Downgrade is not supported on this device\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	ze_bool_t downState = (enabled == true) ? true : false;
	zes_device_action_t pendingAction = {};
	res = zesDevicePciLinkSpeedUpdateExt(device, downState, &pendingAction);
	if (res != ZE_RESULT_SUCCESS) {
		ERR("Failed to set PCIe downgrade state: 0x{:X} ({})\n", res, l0_error_to_string(res));
		return res;
	}

	// Update the output state structure
	state.downgradeSupported = currentState.downgradeSupported;
	state.downgradeEnabled = enabled;
	state.pendingAction = pendingAction;

	DBG("PCIe downgrade state set to: {}\n", enabled ? "enabled" : "disabled");
	DBG("Pending Action: {}\n", pendingAction);

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Checks if a string matches the Bus:Device.Function (BDF) format
 *
 * This function validates whether the provided string conforms to the standard
 * PCI BDF address format (domain:bus:device.function) and matches the device's
 * actual PCI address for device identification and filtering.
 *
 * @param bdf String containing the BDF address to validate
 * @return bool True if the BDF format is valid and matches this device, false otherwise
 */
bool pci::isBDF(const char *bdf)
{
	bool isValid = false;
	std::string bdfS(bdf);
	std::regex regexPattern(R"(([0-9a-fA-F]+):([0-9a-fA-F]+):([0-9a-fA-F]+)\.([0-9a-fA-F]+))");
	std::smatch match;

	if (std::regex_match(bdfS, match, regexPattern)) {
		std::string dmn = match[1];
		std::string b = match[2];
		std::string device = match[3];
		std::string function = match[4];

		if (dmn.length() > 4 || b.length() > 2 || device.length() > 2 || function.length() > 1) {
			ERR("Invalid PCI address format. Correct format example: 1234:05:06.7\n");
		} else {
			DBG("Valid PCI address format: {}\n", bdf);
			if (deviceProperties.pciProps.address.domain == strtoul(dmn.c_str(), nullptr, 16) &&
				deviceProperties.pciProps.address.bus == strtoul(b.c_str(), nullptr, 16) &&
				deviceProperties.pciProps.address.device == strtoul(device.c_str(), nullptr, 16) &&
				deviceProperties.pciProps.address.function == strtoul(function.c_str(), nullptr, 16)) {
				DBG("PCI address matches the device properties.\n");
				isValid = true;
			}
		}
	}
	return isValid;
}

/**
 * @brief  Gets the function type of a device. If function number is 0, it is physical function, otherwise virtual
 * function.
 *
 * @return devFuncType Returns the function type of the device.
 */
devFuncType pci::getFuncType()
{
	TRACING();
	return (func == 0) ? DEVICE_FUNCTION_TYPE_PHYSICAL : DEVICE_FUNCTION_TYPE_VIRTUAL;
}

/**
 * @brief Initializes the PCI management module for a device
 *
 * This function performs initial setup of PCI monitoring capabilities by
 * retrieving PCI properties and current state information for the specified
 * device for subsequent PCI analysis operations.
 *
 * @param device Handle to the device for PCI initialization
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t pci::init(zes_device_handle_t device)
{
	ze_result_t result;
	result = getProperties(device, &deviceProperties.pciProps);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI properties: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	domain = deviceProperties.pciProps.address.domain;
	bus = deviceProperties.pciProps.address.bus;
	dev = deviceProperties.pciProps.address.device;
	func = deviceProperties.pciProps.address.function;
	snprintf(bdfStr, sizeof(bdfStr), "%04x:%02x:%02x.%01x", deviceProperties.pciProps.address.domain,
			 deviceProperties.pciProps.address.bus, deviceProperties.pciProps.address.device,
			 deviceProperties.pciProps.address.function);

	gscupd gsc;
	std::vector<pci_addr_mei_device> devicesVec = gsc.getPCIAddrAndMeiDevices();

	for (const auto &dv : devicesVec) {
		if (dv.pciProps.address.domain == deviceProperties.pciProps.address.domain &&
			dv.pciProps.address.bus == deviceProperties.pciProps.address.bus &&
			dv.pciProps.address.device == deviceProperties.pciProps.address.device &&
			dv.pciProps.address.function == deviceProperties.pciProps.address.function) {
			DBG("Found matching device: {}\n", dv.meiDevicePath.c_str());

			// Found a matching device so copy the meiDevicePath
			deviceProperties.meiDevicePath = dv.meiDevicePath;
			// Also copy the fw status
			deviceProperties.fwStatus = dv.fwStatus;
			break;
		}
	}

	return result;
}

/**
 * @brief Performs comprehensive PCI monitoring runtime operations
 *
 * This function executes a complete PCI monitoring cycle including statistics
 * retrieval, state checking, and BAR information gathering for thorough PCI
 * performance analysis and system topology assessment.
 *
 * @param device Handle to the device for PCI operations
 * @return ze_result_t ZE_RESULT_SUCCESS on successful execution, error code otherwise
 */
ze_result_t pci::zesRun(zes_device_handle_t device)
{
	zes_pci_stats_t pciStats = {};
	zes_pci_link_status_t pciLinkStatus = ZES_PCI_LINK_STATUS_UNKNOWN;

	getState(device, pciLinkStatus);
	getBars(device);
	getStats(device, &pciStats);

	return ZE_RESULT_SUCCESS;
}
