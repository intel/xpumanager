/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysprocess.h"
#include <vector>

/**
 * @brief Gets the current state of all processes using a device
 *
 * This function retrieves comprehensive information about all processes currently
 * utilizing the specified device, including process IDs, memory usage (shared and
 * private), and process names for system monitoring and resource management.
 *
 * @param device Handle to the device
 * @param processList Pointer to vector to store process state information
 * @return ze_result_t ZE_RESULT_SUCCESS on successful process enumeration, error code otherwise
 */
ze_result_t process::getState(zes_device_handle_t device, std::vector<zes_process_state_t> *processList)
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
	for (const auto &ps : *processList) {
		DBG("    - Process ID: %d\n", ps.processId);
		DBG("    - Name: %s\n", GETPROCESSNAME(ps.processId).c_str());
		DBG("    - Shared Size: %" PRIu64 " KB\n", (ps.sharedSize / 1024));
		DBG("    - Memory Size: %" PRIu64 " KB\n", (ps.memSize / 1024));
	}
	return result;
}

/**
 * @brief Performs comprehensive process monitoring runtime operations
 *
 * This function executes a complete process monitoring cycle to retrieve
 * information about all processes currently using the device, providing
 * real-time process utilization and resource consumption data.
 *
 * @param device Handle to the device for process monitoring
 * @return ze_result_t ZE_RESULT_SUCCESS on successful execution, error code otherwise
 */
ze_result_t process::zesRun(zes_device_handle_t device)
{
	TRACING();

	std::vector<zes_process_state_t> processList;
	return getState(device, &processList);
}
