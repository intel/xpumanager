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
#include <unordered_set>
#include <vector>

/**
 * @brief Queries all processes currently using @p device.
 *
 * Calls zesDeviceProcessesGetState() to enumerate active processes, then narrows
 * the result to processes that are genuinely clients of this device.
 * applyFdinfoCorrections() first rewrites each entry from the OS's own
 * per-process accounting, where the platform keeps one, and drops processes it
 * shows never ran work here; see oal/process_platform.h for what each platform
 * provides.  The filter below then removes entries that have neither engines nor
 * a memory footprint.
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

	// Retry when a new process starts between the count query and the data
	// fetch: the driver returns INVALID_SIZE and updates processCount to the
	// new required size, so we can resize and try again immediately.
	constexpr int maxRetries = 3;
	for (int attempt = 0; attempt < maxRetries; ++attempt) {
		processList->assign(processCount, [] {
			zes_process_state_t ps{};
			ps.stype = ZES_STRUCTURE_TYPE_PROCESS_STATE;
			return ps;
		}());
		result = zesDeviceProcessesGetState(device, &processCount, processList->data());
		if (result != ZE_RESULT_ERROR_INVALID_SIZE) {
			break;
		}
		processCount = 0;
		const ze_result_t recount = zesDeviceProcessesGetState(device, &processCount, nullptr);
		if (recount != ZE_RESULT_SUCCESS) {
			result = recount;
			break;
		}
	}
	if (result != ZE_RESULT_SUCCESS) {
		if (result == ZE_RESULT_ERROR_INVALID_SIZE) {
			DBG("Process list changed faster than {} retries could keep up; skipping this cycle\n", maxRetries);
		} else {
			ERR("Failed to get process states: 0x{:X} ({})\n", result, l0_error_to_string(result));
		}
		return result;
	}

	// PIDs whose engines mask applyFdinfoCorrections replaced with verified activity.
	// Stays empty where it could not run, which is what the filter below keys off.
	std::unordered_set<uint32_t> engineVerified;

	if (!processList->empty()) {
		// Correct memSize and engines before filtering so the filter operates on
		// accurate per-PID values rather than driver-reported ones.
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

			// A device with at least one DEVICE-local memory module is a dGPU, where
			// only device-local memory counts toward a process's footprint; an iGPU
			// has none and counts shared memory instead.  When enumeration fails
			// (count query or handle fetch) the GPU type is unknown and memKind stays
			// nullopt, which leaves memSize as Level Zero reported it rather than
			// risk zeroing a valid value on a dGPU.
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

			// Not gated on memKind: the engine filter does not depend on the GPU
			// type, so a failed memory-module query must not leave every process
			// that never ran work here in the list.
			engineVerified = applyFdinfoCorrections(bdf, processList, memKind);
		}
	}

	// Remove processes with no memory on this device and no engine activity.
	// applyFdinfoCorrections corrects memSize where it can, zeroing phantom entries
	// that the driver over-attributed; any remaining positive value is a verified
	// allocation and is preserved regardless of size.
	//
	// ZES_ENGINE_TYPE_FLAG_OTHER on its own means opposite things depending on where
	// the mask came from, so the two cases cannot share one rule. Straight from the
	// driver it means "no real workload" and XPUM-1242 discards it. Rewritten by
	// applyFdinfoCorrections it means activity on an engine class this build cannot
	// name, which is proof of work -- discarding that would drop a running process.
	// Only engineVerified PIDs carry the second meaning.
	std::erase_if(*processList, [&engineVerified](const zes_process_state_t &ps) {
		const bool maskFromFdinfo = engineVerified.contains(ps.processId);
		const bool hasEngines =
			ps.engines != 0 &&
			(maskFromFdinfo || ps.engines != static_cast<zes_engine_type_flags_t>(ZES_ENGINE_TYPE_FLAG_OTHER));
		if (hasEngines || ps.memSize > 0 || ps.sharedSize > 0) {
			return false;
		}
		DBG("  - PID {} filtered: no memory or engine activity on this device\n", ps.processId);
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
