/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Windows stub for proc_fdinfo — /proc does not exist on Windows.
 * All functions return empty / zero / all-nullopt results so callers
 * degrade gracefully without any platform guards.
 */

#include "../proc_fdinfo.h"

namespace fdinfo {

std::vector<ProcessSnapshot> capture(const std::string & /*pciAddr*/, const std::string & /*procRoot*/) { return {}; }

PidUtilMap delta(const std::vector<ProcessSnapshot> & /*before*/, const std::vector<ProcessSnapshot> & /*after*/)
{
	return {};
}

ProcUtil toProcUtil(const EngineUtilMap & /*engineUtils*/) { return {}; }

/**
 * @brief Stub: Windows has no fdinfo engine keys to map.
 *
 * @param[in] rawEngine  Unused.
 *
 * @retval 0  Always.
 */
uint64_t engineFlagForKey(std::string_view /*rawEngine*/) { return 0; }

/**
 * @brief Stub: Windows has no fdinfo keys to classify.
 *
 * @param[in] key  Unused.
 *
 * @retval std::nullopt  Always.
 */
std::optional<EngineKey> parseEngineKey(std::string_view /*key*/) { return std::nullopt; }

uint64_t enginesFromSnapshot(const ProcessSnapshot & /*snap*/) { return 0; }

ProcUtil aggregateDeviceUtil(const std::vector<ProcessSnapshot> & /*before*/,
							 const std::vector<ProcessSnapshot> & /*after*/)
{
	return {};
}

std::unordered_map<std::string, uint32_t> engineCountsPerClass(const std::string & /*pciAddr*/,
															   const std::string & /*procRoot*/)
{
	return {};
}

} // namespace fdinfo
