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
#ifndef _MEMORY_H
#define _MEMORY_H

#include "sysman.h"

class LIBXPUM_API memory : public sysman
{
private:
	uint32_t memoryModulesCount;
	zes_mem_handle_t *memoryModules;

public:
	memory() : memoryModulesCount(0), memoryModules(nullptr) {}
	~memory();
	ze_result_t enumMemoryModules(zes_device_handle_t device);
	ze_result_t getMemoryProperties(zes_mem_handle_t memhandle, zes_mem_properties_t *properties);
	ze_result_t getState(zes_mem_handle_t memhandle, zes_mem_state_t *state);
	ze_result_t getBandwidth(zes_mem_handle_t memhandle);
	ze_result_t getMemorySize(uint64_t *size);
	ze_result_t getMemoryChannels(uint32_t *channels);
	ze_result_t getMemoryBusWidth(uint32_t *busWidth);

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device);
};

#endif