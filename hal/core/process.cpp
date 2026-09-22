/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysprocess.h"
#include "process_platform.h"
#include "utility/compat/format.h"
#include "utility/units/units.h"
#include <algorithm>
#include <optional>
#include <vector>

/**
 * @brief Queries all processes currently using @p device.
 *
 * Calls zesDeviceProcessesGetState() to enumerate active processes, keeping
 * only entries with memory allocated on this device (memSize > 0 or
 * sharedSize > 0).  Processes that open a context without allocating memory
 * are filtered out so each GPU only lists PIDs with an actual footprint there.
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
		// UNSUPPORTED_FEATURE is expected on the sysman-only path (no compute context).
		if (result != ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
			ERR("Failed to get process count: 0x{:X} ({})\n", result, l0_error_to_string(result));
		} else {
			DBG("Failed to get process count: 0x{:X} ({})\n", result, l0_error_to_string(result));
		}
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

	if (!processList->empty()) {
		// Correct memSize via fdinfo before filtering so the filter operates on
		// accurate per-PID values rather than driver-reported ones.
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

			// A device with at least one DEVICE-local memory module is a dGPU.
			// On dGPUs, fixProcessMemSize uses only drm-total-local/vram bytes so
			// GTT-only cross-GPU phantom entries are correctly zeroed and filtered.
			// When enumeration fails (count query or handle fetch) the GPU type is
			// unknown; skip fdinfo correction to avoid zeroing L0 values on a dGPU.
			uint32_t memCount = 0;
			std::optional<MemKind> memKind;
			if (zesDeviceEnumMemoryModules(device, &memCount, nullptr) == ZE_RESULT_SUCCESS) {
				if (memCount == 0) {
					memKind = MemKind::Shared; // iGPU: no device-local memory modules
				} else {
					std::vector<zes_mem_handle_t> memHandles(memCount);
					if (zesDeviceEnumMemoryModules(device, &memCount, memHandles.data()) == ZE_RESULT_SUCCESS) {
						memKind = std::ranges::any_of(memHandles,
													  [](zes_mem_handle_t h) {
														  zes_mem_properties_t p{};
														  p.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
														  return zesMemoryGetProperties(h, &p) == ZE_RESULT_SUCCESS &&
																 p.location == ZES_MEM_LOC_DEVICE;
													  })
									  ? MemKind::Local
									  : MemKind::Shared;
					}
				}
			}

			if (memKind) {
				fixProcessMemSize(bdf, processList, *memKind);
			}
		}
	}

	// Remove processes with no memory on this device and no real engine activity.
	// fixProcessMemSize corrects memSize via fdinfo, zeroing phantom entries that
	// the driver over-attributed; any remaining positive value is a verified
	// allocation and is preserved regardless of size.
	std::erase_if(*processList, [](const zes_process_state_t &ps) {
		const bool hasRealEngines =
			ps.engines != 0 && ps.engines != static_cast<zes_engine_type_flags_t>(ZES_ENGINE_TYPE_FLAG_OTHER);
		if (hasRealEngines || ps.memSize > 0 || ps.sharedSize > 0) {
			return false;
		}
		DBG("  - PID {} filtered: no memory on this device\n", ps.processId);
		return true;
	});

	processCount = static_cast<uint32_t>(processList->size());
	DBG("  - Device has {} processes\n", processCount);
	for (const auto &ps : *processList) {
		DBG("    - Process ID: {}\n", ps.processId);
		DBG("    - Name: {}\n", GETPROCESSNAME(ps.processId).c_str());
		DBG("    - Shared Size: {} KB\n", xpum::units::Bytes{ps.sharedSize}.kibibytes());
		DBG("    - Memory Size: {} KB\n", xpum::units::Bytes{ps.memSize}.kibibytes());
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
