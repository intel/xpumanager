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
#include "fabric.h"

fabric::~fabric()
{
	if (ports)
	{
		delete[] ports;
		ports = nullptr;
	}
}

ze_result_t fabric::enumFabricPorts(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumFabricPorts(device, &portCount, nullptr);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to enumerate fabric ports: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	DBG("Device has %d fabric ports\n", portCount);

	if (portCount == 0)
		return ZE_RESULT_SUCCESS;

	ports = new zes_fabric_port_handle_t[portCount];
	result = zesDeviceEnumFabricPorts(device, &portCount, ports);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get fabric port handles: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	return result;
}

ze_result_t fabric::portGetProperties(zes_fabric_port_handle_t hFabricPort)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	DBG("Fabric Ports Properties:\n");

	zes_fabric_port_properties_t portProperties = {};
	result = zesFabricPortGetProperties(hFabricPort, &portProperties);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get fabric port properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("  - Fabric Id: %d\n", portProperties.portId.fabricId);
	DBG("  - Attach Id: %d\n", portProperties.portId.attachId);
	DBG("  - Port Number: %d\n", portProperties.portId.portNumber);
	DBG("  - Port SType: %d\n", portProperties.stype);
	DBG("  - Port model: %s\n", portProperties.model);
	DBG("  - Port OnSubdevice ID: %d\n", portProperties.onSubdevice);
	DBG("  - Port Subdevice ID: %d\n", portProperties.subdeviceId);
	DBG("  - Port maxRxSpeed bitRate: %" PRIu64 "\n", portProperties.maxRxSpeed.bitRate);
	DBG("  - Port maxRxSpeed width: %d\n", portProperties.maxRxSpeed.width);
	DBG("  - Port maxTxSpeed bitRate: %" PRIu64 "\n", portProperties.maxTxSpeed.bitRate);
	DBG("  - Port maxTxSpeed width: %d\n", portProperties.maxTxSpeed.width);

	return result;
}

ze_result_t fabric::portGetLinkType(zes_fabric_port_handle_t hFabricPort)
{
	zes_fabric_link_type_t linkType;
	// Get the link type of the fabric port
	ze_result_t result = zesFabricPortGetLinkType(hFabricPort, &linkType);
	if (result == ZE_RESULT_SUCCESS)
	{
		DBG("Fabric Port Link Type: %s\n", linkType.desc);
	}
	else
	{
		ERR("Error getting fabric port link type: 0x%X (%s)\n", result, l0_error_to_string(result));
	}
	return result;
}

ze_result_t fabric::portGetConfig(zes_fabric_port_handle_t hFabricPort)
{
	zes_fabric_port_config_t portConfig = {};
	ze_result_t result = zesFabricPortGetConfig(hFabricPort, &portConfig);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get fabric port configuration: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Fabric Port Configuration:");
	DBG("  - Enabled: %s\n", (portConfig.enabled ? "Yes" : "No"));
	DBG("  - Beaconing: %s\n", (portConfig.beaconing ? "Enabled" : "Disabled"));

	return result;
}

ze_result_t fabric::portGetState(zes_fabric_port_handle_t hFabricPort)
{
	zes_fabric_port_state_t portState = {};
	ze_result_t result = zesFabricPortGetState(hFabricPort, &portState);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get fabric port state: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Fabric Port State:");
	DBG("  - Status: %d\n", portState.status);
	DBG("  - Quality Issues: %d\n", portState.qualityIssues);
	DBG("  - Failure Reasons: %d\n", portState.failureReasons);
	DBG("  - Remote Port Id - Fabric Id: %d\n", portState.remotePortId.fabricId);
	DBG("  - Remote Port Id - Attach Id: %d\n", portState.remotePortId.attachId);
	DBG("  - Remote Port Id - Port Number: %d\n", portState.remotePortId.portNumber);
	DBG("  - Rx Speed - Bit Rate: %" PRIu64 "\n", portState.rxSpeed.bitRate);
	DBG("  - Rx Speed - Width: %d\n", portState.rxSpeed.width);
	DBG("  - Tx Speed - Bit Rate: %" PRIu64 "\n", portState.txSpeed.bitRate);
	DBG("  - Tx Speed - Width: %d\n", portState.txSpeed.width);

	return result;
}

ze_result_t fabric::portGetThroughput(zes_fabric_port_handle_t hFabricPort)
{
	zes_fabric_port_throughput_t throughput = {};
	ze_result_t result = zesFabricPortGetThroughput(hFabricPort, &throughput);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get fabric port throughput: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Fabric Port Throughput:");
	DBG("  - Rx Counter: %" PRIu64 "\n", throughput.rxCounter);
	DBG("  - Tx Counter: %" PRIu64 "\n", throughput.txCounter);
	DBG("  - Timestamp: %" PRIu64 "\n", throughput.timestamp);

	return result;
}

ze_result_t fabric::portGetFabricErrorCounters(zes_fabric_port_handle_t hFabricPort)
{
	zes_fabric_port_error_counters_t errorCounters = {};
	ze_result_t result = zesFabricPortGetFabricErrorCounters(hFabricPort, &errorCounters);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get fabric port error counters: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Fabric Port Error Counters:");
	DBG("  - Link Failure Count: %" PRIu64 "\n", errorCounters.linkFailureCount);
	DBG("  - Link Degrade Count: %" PRIu64 "\n", errorCounters.linkDegradeCount);
	DBG("  - Firmware reported Error Count: %" PRIu64 "\n", errorCounters.fwErrorCount);
	DBG("  - Firmware Communication Error Count: %" PRIu64 "\n", errorCounters.fwCommErrorCount);

	return result;
}

ze_result_t fabric::portGetMultiPortThroughput(zes_device_handle_t device, uint32_t count)
{
	if (count == 0 || device == nullptr)
	{
		ERR("Invalid fabric port handles or count.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	zes_fabric_port_throughput_t *throughputs = new zes_fabric_port_throughput_t[count];
	ze_result_t result = zesFabricPortGetMultiPortThroughput(device, count, ports, &throughputs);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get multi-port throughput: 0x%X (%s)\n", result, l0_error_to_string(result));
		delete[] throughputs;
		return result;
	}

	DBG("Multi-Port Throughput:");
	for (uint32_t i = 0; i < count; i++)
	{
		DBG("Port %d:\n", i);
		DBG("  - Rx Counter: %" PRIu64 "\n", throughputs[i].rxCounter);
		DBG("  - Tx Counter: %" PRIu64 "\n", throughputs[i].txCounter);
		DBG("  - Timestamp: %" PRIu64 "\n", throughputs[i].timestamp);
	}

	delete[] throughputs;
	return result;
}

ze_result_t fabric::zesRun(zes_device_handle_t device)
{
	ze_result_t result = enumFabricPorts(device);
	if (result != ZE_RESULT_SUCCESS)
		return result;

	for (uint32_t i = 0; i < portCount; i++)
	{
		result = portGetProperties(ports[i]);
		if (result != ZE_RESULT_SUCCESS)
		{
			return result;
		}

		result = portGetLinkType(ports[i]);
		if (result != ZE_RESULT_SUCCESS)
		{
			return result;
		}

		result = portGetConfig(ports[i]);
		if (result != ZE_RESULT_SUCCESS)
		{
			return result;
		}

		result = portGetState(ports[i]);
		if (result != ZE_RESULT_SUCCESS)
		{
			return result;
		}

		result = portGetThroughput(ports[i]);
		if (result != ZE_RESULT_SUCCESS)
		{
			return result;
		}

		result = portGetFabricErrorCounters(ports[i]);
		if (result != ZE_RESULT_SUCCESS)
		{
			return result;
		}
	}

	result = portGetMultiPortThroughput(device, portCount);
	if (portCount && result != ZE_RESULT_SUCCESS)
	{
		return result;
	}

	return result;
}