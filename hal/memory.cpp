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
#include "memory.h"

memory::~memory()
{
	if (memoryModules)
	{
		delete[] memoryModules;
		memoryModules = nullptr;
	}
}

ze_result_t memory::enumMemoryModules(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumMemoryModules(device, &memoryModulesCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || memoryModulesCount == 0)
	{
		ERR("Failed to enumerate Memory modules. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	memoryModules = new zes_mem_handle_t[memoryModulesCount];
	result = zesDeviceEnumMemoryModules(device, &memoryModulesCount, memoryModules);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get  Memory modules. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Found %d memory modules\n", memoryModulesCount);
	return result;
}

ze_result_t memory::getMemoryProperties(zes_mem_handle_t memhandle)
{
	zes_mem_properties_t properties;
	ze_result_t result = zesMemoryGetProperties(memhandle, &properties);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get memory properties. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Memory properties retrieved successfully.\n");

	DBG("Memory type: ");
	switch (properties.type)
	{
	case ZES_MEM_TYPE_HBM:
		DBG("HBM\n");
		break;
	case ZES_MEM_TYPE_DDR:
		DBG("DDR\n");
		break;
	case ZES_MEM_TYPE_DDR3:
		DBG("DDR3\n");
		break;
	case ZES_MEM_TYPE_DDR4:
		DBG("DDR4\n");
		break;
	case ZES_MEM_TYPE_DDR5:
		DBG("DDR5\n");
		break;
	case ZES_MEM_TYPE_LPDDR:
		DBG("LPDDR\n");
		break;
	case ZES_MEM_TYPE_LPDDR3:
		DBG("LPDDR3\n");
		break;
	case ZES_MEM_TYPE_LPDDR4:
		DBG("LPDDR4\n");
		break;
	case ZES_MEM_TYPE_LPDDR5:
		DBG("LPDDR5\n");
		break;
	case ZES_MEM_TYPE_SRAM:
		DBG("SRAM\n");
		break;
	case ZES_MEM_TYPE_L1:
		DBG("L1\n");
		break;
	case ZES_MEM_TYPE_L3:
		DBG("L3\n");
		break;
	case ZES_MEM_TYPE_GRF:
		DBG("GRF\n");
		break;
	case ZES_MEM_TYPE_SLM:
		DBG("SLM\n");
		break;
	case ZES_MEM_TYPE_GDDR4:
		DBG("GDDR4\n");
		break;
	case ZES_MEM_TYPE_GDDR5:
		DBG("GDDR5\n");
		break;
	case ZES_MEM_TYPE_GDDR5X:
		DBG("GDDR5X\n");
		break;
	case ZES_MEM_TYPE_GDDR6:
		DBG("GDDR6\n");
		break;
	case ZES_MEM_TYPE_GDDR6X:
		DBG("GDDR6X\n");
		break;
	case ZES_MEM_TYPE_GDDR7:
		DBG("GDDR7\n");
		break;
	default:
		DBG("Other\n");
		break;
	}

	return result;
}

ze_result_t memory::getState(zes_mem_handle_t memhandle)
{
	zes_mem_state_t state;
	ze_result_t result = zesMemoryGetState(memhandle, &state);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get memory state. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Memory state retrieved successfully.\n");
	DBG("Free memory: %" PRIu64 "bytes\n", state.free);
	DBG("Size: %" PRIu64 "bytes\n", state.size);
	DBG("Health: ");
	switch (state.health)
	{
	case ZES_MEM_HEALTH_OK:
		DBG("OK\n");
		break;
	case ZES_MEM_HEALTH_DEGRADED:
		DBG("Degraded\n");
		break;
	case ZES_MEM_HEALTH_CRITICAL:
		DBG("Critical\n");
		break;
	case ZES_MEM_HEALTH_REPLACE:
		DBG("Replace\n");
		break;
	default:
		DBG("Unknown\n");
		break;
	}

	return result;
}

ze_result_t memory::getBandwidth(zes_mem_handle_t memhandle)
{
	zes_mem_bandwidth_t bandwidth;
	ze_result_t result = zesMemoryGetBandwidth(memhandle, &bandwidth);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get memory bandwidth. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Memory bandwidth retrieved successfully.\n");
	DBG("Read counter: %" PRIu64 "bytes\n", bandwidth.readCounter);
	DBG("Write counter: %" PRIu64 "bytes\n", bandwidth.writeCounter);
	DBG("Max bandwidth: %" PRIu64 "bytes/sec\n", bandwidth.maxBandwidth);

	return result;
}

ze_result_t memory::zesRun(zes_device_handle_t device)
{
	enumMemoryModules(device);

	for (uint32_t i = 0; i < memoryModulesCount; i++)
	{
		getMemoryProperties(memoryModules[i]);
		getState(memoryModules[i]);
		getBandwidth(memoryModules[i]);
	}

	return ZE_RESULT_SUCCESS;
}