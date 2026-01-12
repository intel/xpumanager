/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _FABRIC_H
#define _FABRIC_H

#include "sysman.h"
#include <vector>

// Add fabric port info structures
struct portInfo
{
	zes_fabric_port_properties_t portProps;
	zes_fabric_port_state_t portState;
	zes_fabric_link_type_t portLink;
	zes_fabric_port_config_t portConf;
};

struct portInfoSet
{
	uint32_t subdeviceId;
	zes_fabric_port_id_t portId;
	bool enabled;
	bool beaconing;
	bool settingEnabled;
	bool settingBeaconing;
};
class LIBXPUM_API fabric : public sysman
{
private:
	zes_fabric_port_handle_t *ports;
	uint32_t portCount;

public:
	fabric() : ports(nullptr), portCount(0) {}
	~fabric();
	ze_result_t enumFabricPorts(zes_device_handle_t device);
	ze_result_t portGetProperties(zes_fabric_port_handle_t hFabricPort, zes_fabric_port_properties_t *properties);
	ze_result_t portGetLinkType(zes_fabric_port_handle_t hFabricPort);
	ze_result_t portGetConfig(zes_fabric_port_handle_t hFabricPort);
	ze_result_t portGetState(zes_fabric_port_handle_t hFabricPort, zes_fabric_port_state_t *state);
	ze_result_t portGetThroughput(zes_fabric_port_handle_t hFabricPort, zes_fabric_port_throughput_t *throughput);
	ze_result_t portGetFabricErrorCounters(zes_fabric_port_handle_t hFabricPort);
	ze_result_t portGetMultiPortThroughput(zes_device_handle_t device, uint32_t count,
										   zes_fabric_port_throughput_t *throughputs);
	ze_result_t getFabricPorts(zes_device_handle_t device, std::vector<portInfo> &portInfoIn);
	ze_result_t setFabricPorts(zes_device_handle_t device, const portInfoSet &portInfoSetIn);
	zes_fabric_port_handle_t getPortHandle(uint32_t index) const;
	uint32_t getPortCount() const { return portCount; }

	ze_result_t setPortConfig(bool enabled);
	ze_result_t setPortBeaconing(bool enabled);

	ze_result_t init(zes_device_handle_t device);
	ze_result_t zesRun(zes_device_handle_t device);
};

#endif