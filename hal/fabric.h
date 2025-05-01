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
#ifndef _FABRIC_H
#define _FABRIC_H

#include "sysman.h"

class LIBXPUM_API fabric : public sysman
{
private:
	zes_fabric_port_handle_t *ports;
	uint32_t portCount;

public:
	fabric() : ports(nullptr), portCount(0) {}
	~fabric();
	ze_result_t enumFabricPorts(zes_device_handle_t device);
	ze_result_t portGetProperties(zes_fabric_port_handle_t hFabricPort);
	ze_result_t portGetLinkType(zes_fabric_port_handle_t hFabricPort);
	ze_result_t portGetConfig(zes_fabric_port_handle_t hFabricPort);
	ze_result_t portGetState(zes_fabric_port_handle_t hFabricPort);
	ze_result_t portGetThroughput(zes_fabric_port_handle_t hFabricPort);
	ze_result_t portGetFabricErrorCounters(zes_fabric_port_handle_t hFabricPort);
	ze_result_t portGetMultiPortThroughput(zes_device_handle_t device, uint32_t count);
	ze_result_t zesRun(zes_device_handle_t device);
};

#endif