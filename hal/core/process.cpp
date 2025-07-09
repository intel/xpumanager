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

#include "sysprocess.h"
#include <vector>

using namespace std;

ze_result_t process::getState(zes_device_handle_t device, vector<zes_process_state_t> *processList)
{
	// Get processes running on the device
	uint32_t processCount = 0;
	ze_result_t result = zesDeviceProcessesGetState(device, &processCount, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get process count: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	processList->clear();
	processList->resize(processCount);
	result = zesDeviceProcessesGetState(device, &processCount, processList->data());
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get process states: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("  - Device has %d processes\n", processCount);
	for (const auto &process : *processList) {
		DBG("    - Process ID: %d\n", process.processId);
		DBG("    - Name: %s\n", GETPROCESSNAME(process.processId).c_str());
		DBG("    - Shared Size: %" PRIu64 " KB\n", (process.sharedSize / 1024));
		DBG("    - Memory Size: %" PRIu64 " KB\n", (process.memSize / 1024));
	}
	return result;
}

ze_result_t process::zesRun(zes_device_handle_t device)
{
	TRACING();

	vector<zes_process_state_t> processList;
	return getState(device, &processList);
}
