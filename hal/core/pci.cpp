/*
 * Copyright © 2025 Intel Corporation
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
#include "pci.h"
#include <assert.h>
#include <gscupd.h>
#include <regex>
#include <string>
#include <vector>

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
		ERR("Failed to get PCI properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("  - PCI Properties:\n");
	DBG("    - Address: %d:%d:%d.%d\n", pciProps->address.domain, pciProps->address.bus, pciProps->address.device,
		pciProps->address.function);
	DBG("    - Max Speed: %d Gen, %d lanes, Max bandwidth: %" PRIu64 "\n", pciProps->maxSpeed.gen,
		pciProps->maxSpeed.width, pciProps->maxSpeed.maxBandwidth);

	DBG("    - haveBandwidthCounters: %d\n", pciProps->haveBandwidthCounters);
	DBG("    - havePacketCounters: %d\n", pciProps->havePacketCounters);
	DBG("    - haveReplayCounters: %d\n", pciProps->haveReplayCounters);
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
		ERR("Failed to get PCI state: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	pciLinkStatus = pciState.status;

	DBG("  - PCI State:");
	DBG("    - Status: %d\n", pciState.status);
	DBG("    - Stability Issues: %d\n", pciState.stabilityIssues);
	DBG("    - Quality Issues: %d\n", pciState.qualityIssues);
	DBG("    - Current Speed: %d Gen, %d lanes, %" PRIu64 "Max bandwidth\n", pciState.speed.gen, pciState.speed.width,
		pciState.speed.maxBandwidth);
	return result;
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
		ERR("Failed to get PCI BAR count: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	std::vector<zes_pci_bar_properties_t> bars(barCount);
	result = zesDevicePciGetBars(device, &barCount, bars.data());
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get PCI BAR properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("  - PCI BARs:\n");
	for (const auto &bar : bars) {
		DBG("    - Index: %d\n", bar.index);
		DBG("    - Base Address: 0x%" PRIx64 "\n", bar.base);
		DBG("    - Size: 0x%" PRIx64 "bytes\n", bar.size);
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
		ERR("Failed to get PCI stats: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("  - PCI Stats:");
	DBG("    - Timestamp: %" PRIu64 "\n", pciStats->timestamp);
	DBG("    - Replay counter: %" PRIu64 "\n", pciStats->replayCounter);
	DBG("    - Packet counter: %" PRIu64 "\n", pciStats->packetCounter);
	DBG("    - Received Bytes: %" PRIu64 "\n", pciStats->rxCounter);
	DBG("    - Transmitted Bytes: %" PRIu64 "\n", pciStats->txCounter);

	return result;
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
	std::regex regexPattern(R"((\d+):(\d+):(\d+)\.(\d+))");
	std::smatch match;

	if (std::regex_match(bdfS, match, regexPattern)) {
		std::string dmn = match[1];
		std::string b = match[2];
		std::string device = match[3];
		std::string function = match[4];

		if (dmn.length() > 4 || b.length() > 2 || device.length() > 2 || function.length() > 1) {
			ERR("Invalid PCI address format. Correct format example: 1234:05:06.7\n");
		} else {
			DBG("Valid PCI address format: %s\n", bdf);
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
		ERR("Failed to get PCI properties: 0x%X (%s)\n", result, l0_error_to_string(result));
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
			DBG("Found matching device: %s\n", dv.meiDevicePath.c_str());

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
