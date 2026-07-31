/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysprocess.h"
#include "process_platform.h"
#include "utility/compat/format.h"
#include <vector>

/**
 * @brief Queries all processes currently using @p device.
 *
 * Calls zesDeviceProcessesGetState() to enumerate active processes, removes
 * entries with no known engine activity (engines == 0 or OTHER-only), then
 * replaces memSize with fdinfo-based values that correctly deduplicate by
 * drm-client-id across both multiple fds and forked children.
 *
 * @param[in]  device       Sysman device handle.
 * @param[out] processList  Cleared and repopulated on success.
 *
 * @retval ZE_RESULT_SUCCESS  Enumeration and correction completed without error.
 * @retval ZE_RESULT_ERROR_*  Level Zero error; @p processList may be empty.
 */
ze_result_t process::getState(zes_device_handle_t device, std::vector<zes_process_state_t> *processList)
{
	uint32_t processCount = 0;
	ze_result_t result = zesDeviceProcessesGetState(device, &processCount, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get process count: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	processList->clear();
	processList->resize(processCount);
	for (auto &procState : *processList) {
		procState.stype = ZES_STRUCTURE_TYPE_PROCESS_STATE;
		procState.pNext = nullptr;
	}
	result = zesDeviceProcessesGetState(device, &processCount, processList->data());
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get process states: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	std::erase_if(*processList, [](const zes_process_state_t &ps) {
		return ps.engines == 0 || ps.engines == ZES_ENGINE_TYPE_FLAG_OTHER;
	});

	if (!processList->empty()) {
		// Fix memSize by reading fdinfo and deduplicating by drm-client-id.
		// No-op on platforms without /proc (see win/process_platform.cpp).
		zes_pci_properties_t pciProps{};
		pciProps.stype = ZES_STRUCTURE_TYPE_PCI_PROPERTIES;
		const ze_result_t pciResult = zesDevicePciGetProperties(device, &pciProps);
		if (pciResult != ZE_RESULT_SUCCESS) {
			ERR("zesDevicePciGetProperties failed: 0x{:X} ({})\n", static_cast<uint32_t>(pciResult),
				l0_error_to_string(pciResult));
		} else {
			const std::string bdf =
				xpum::compat::format("{:04x}:{:02x}:{:02x}.{:x}", pciProps.address.domain, pciProps.address.bus,
									 pciProps.address.device, pciProps.address.function);
			fixProcessMemSize(bdf, processList);
		}
	}

	processCount = static_cast<uint32_t>(processList->size());
	DBG("  - Device has {} processes\n", processCount);
	for (const auto &ps : *processList) {
		DBG("    - Process ID: {}\n", ps.processId);
		DBG("    - Name: {}\n", GETPROCESSNAME(ps.processId).c_str());
		DBG("    - Shared Size: {} KB\n", (ps.sharedSize / 1024));
		DBG("    - Memory Size: {} KB\n", (ps.memSize / 1024));
		DBG("    - Engines:\n");
		printEngines(ps.engines);
	}
	return result;
}

/**
 * @brief Runs a full process-monitoring cycle (called by the sysman loop).
 *
 * @param[in] device  Sysman device handle.
 *
 * @retval ZE_RESULT_SUCCESS  Cycle completed without error.
 * @retval ZE_RESULT_ERROR_*  Propagated from getState().
 */
ze_result_t process::zesRun(zes_device_handle_t device)
{
	TRACING();

	std::vector<zes_process_state_t> processList;
	return getState(device, &processList);
}
