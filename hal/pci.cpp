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
#include <vector>
#include <assert.h>
#include <regex>
#include <string>
#include "pci.h"

using namespace std;

ze_result_t pci::getProperties(zes_device_handle_t device, zes_pci_properties_t *pciProps)
{
	assert(pciProps);
	if (!pciProps)
	{
		ERR("Invalid argument: pciProps is null\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	ze_result_t result = zesDevicePciGetProperties(device, pciProps);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get PCI properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("  - PCI Properties:\n");
	DBG("    - Address: %d:%d:%d.%d\n", pciProps->address.domain,
		pciProps->address.bus,
		pciProps->address.device,
		pciProps->address.function);
	DBG("    - Max Speed: %d Gen, %d lanes, Max bandwidth: %" PRIu64 "\n",
		pciProps->maxSpeed.gen,
		pciProps->maxSpeed.width,
		pciProps->maxSpeed.maxBandwidth);

	DBG("    - haveBandwidthCounters: %d\n", pciProps->haveBandwidthCounters);
	DBG("    - havePacketCounters: %d\n", pciProps->havePacketCounters);
	DBG("    - haveReplayCounters: %d\n", pciProps->haveReplayCounters);
	return result;
}

ze_result_t pci::getState(zes_device_handle_t device)
{
	zes_pci_state_t pciState = {};
	ze_result_t result = zesDevicePciGetState(device, &pciState);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get PCI state: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("  - PCI State:");
	DBG("    - Status: %d\n", pciState.status);
	DBG("    - Stability Issues: %d\n", pciState.stabilityIssues);
	DBG("    - Quality Issues: %d\n", pciState.qualityIssues);
	DBG("    - Current Speed: %d Gen, %d lanes, %" PRIu64 "Max bandwidth\n",
		pciState.speed.gen,
		pciState.speed.width,
		pciState.speed.maxBandwidth);
	return result;
}

ze_result_t pci::getBars(zes_device_handle_t device)
{
	// This function is currently a placeholder.
	// Get PCI BARs of each device
	uint32_t barCount = 0;
	ze_result_t result = zesDevicePciGetBars(device, &barCount, nullptr);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get PCI BAR count: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	vector<zes_pci_bar_properties_t> bars(barCount);
	result = zesDevicePciGetBars(device, &barCount, bars.data());
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get PCI BAR properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("  - PCI BARs:\n");
	for (const auto &bar : bars)
	{
		DBG("    - Index: %d\n", bar.index);
		DBG("    - Base Address: 0x%" PRIx64 "\n", bar.base);
		DBG("    - Size: 0x%" PRIx64 "bytes\n", bar.size);
		DBG("    - Type: ");
		if (bar.type == ZES_PCI_BAR_TYPE_MMIO)
		{
			DBG("MMIO");
		}
		else if (bar.type == ZES_PCI_BAR_TYPE_ROM)
		{
			DBG("ROM");
		}
		else if (bar.type == ZES_PCI_BAR_TYPE_MEM)
		{
			DBG("MEM");
		}
		else
		{
			DBG("Other");
		}
		DBG("\n");
	}
	return result;
}

ze_result_t pci::getStats(zes_device_handle_t device)
{
	zes_pci_stats_t pciStats = {};
	ze_result_t result = zesDevicePciGetStats(device, &pciStats);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get PCI stats: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("  - PCI Stats:");
	DBG("    - Timestamp: %" PRIu64 "\n", pciStats.timestamp);
	DBG("    - Replay counter: %" PRIu64 "\n", pciStats.replayCounter);
	DBG("    - Packet counter: %" PRIu64 "\n", pciStats.packetCounter);
	DBG("    - Received Bytes: %" PRIu64 "\n", pciStats.rxCounter);
	DBG("    - Transmitted Bytes: %" PRIu64 "\n", pciStats.txCounter);

	return result;
}

ze_result_t pci::init(ze_device_handle_t device)
{
	return getProperties(device, &pciProperties);
}

ze_result_t pci::zesRun(zes_device_handle_t device)
{
	getState(device);
	getBars(device);
	getStats(device);

	return ZE_RESULT_SUCCESS;
}
