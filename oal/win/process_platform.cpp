/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "process_platform.h"
#include <vector>

// Windows has no /proc/fdinfo equivalent; zesDeviceProcessesGetState values are used as-is.

std::vector<std::string> deviceNodesForBdf([[maybe_unused]] const std::string &bdf,
										   [[maybe_unused]] const std::filesystem::path &sysRoot)
{
	return {};
}

FdinfoResult vramFromFdinfo([[maybe_unused]] uint32_t pid, [[maybe_unused]] const std::vector<std::string> &deviceNodes,
							[[maybe_unused]] const std::string &procRoot,
							[[maybe_unused]] std::unordered_set<uint64_t> *globalSeenIds)
{
	return {};
}

/**
 * @brief No-op: Windows keeps no per-process GPU accounting xpu-smi can read.
 *
 * In practice this is never reached with a non-empty list: the WDDM sysman
 * backend returns ZE_RESULT_ERROR_UNSUPPORTED_FEATURE from
 * zesDeviceProcessesGetState(), so process::getState() returns before calling it.
 * Should Level Zero start reporting processes here, its memSize and engines are
 * used exactly as reported.
 *
 * @param[in]     bdf          Unused.
 * @param[in,out] processList  Left unchanged.
 * @param[in]     memKind      Unused.
 *
 * @return Always empty: no engines mask is ever rewritten, so the caller treats
 *         every value as the driver's.
 */
std::unordered_set<uint32_t> applyFdinfoCorrections([[maybe_unused]] const std::string &bdf,
													[[maybe_unused]] std::vector<zes_process_state_t> *processList,
													[[maybe_unused]] std::optional<MemKind> memKind)
{
	return {};
}
