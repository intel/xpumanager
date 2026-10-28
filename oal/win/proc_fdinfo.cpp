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

/**
 * @brief Stub: Windows has no fdinfo, so no device has DRM clients.
 *
 * @param[in] procRoot  Unused.
 *
 * @retval empty  Always.
 */
DeviceSnapshots captureAll(const std::string & /*procRoot*/) { return {}; }

/**
 * @brief Stub: captureAll() never reports a device on Windows.
 *
 * @param[in] all      Unused.
 * @param[in] pciAddr  Unused.
 *
 * @retval empty vector  Always.
 */
const std::vector<ProcessSnapshot> &snapshotsFor(const DeviceSnapshots & /*all*/, std::string_view /*pciAddr*/)
{
	static const std::vector<ProcessSnapshot> none;
	return none;
}

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

std::unordered_map<uint32_t, bool> ranWorkBetween(const std::vector<ProcessSnapshot> & /*before*/,
												  const std::vector<ProcessSnapshot> & /*after*/,
												  double /*minBusyFraction*/)
{
	return {};
}

std::unordered_map<uint32_t, uint64_t> lifetimeBusyNs(const std::vector<ProcessSnapshot> & /*before*/,
													  const std::vector<ProcessSnapshot> & /*after*/)
{
	return {};
}

std::optional<uint64_t> processAgeNs(uint32_t /*pid*/, const std::string & /*procRoot*/) { return std::nullopt; }

uint32_t drmDeviceCount(uint32_t /*pid*/, const std::unordered_set<std::string> & /*gpuPciAddrs*/,
						const std::string & /*procRoot*/)
{
	return 0;
}

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
