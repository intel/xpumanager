/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Per-process GPU engine utilisation via /proc/<pid>/fdinfo — Linux only.
 *
 * The DRM subsystem exposes per-client engine-busy counters in fdinfo:
 *   xe driver  : drm-cycles-<eng> / drm-total-cycles-<eng>  (cycle ratio)
 *   i915 driver: drm-engine-<eng>: <ns> ns                  (wall-clock ratio)
 *
 * Usage:
 *   const std::string bdf = devPciAddr(dev);   // IAL helper — see ial/cmn/cmds.h
 *   auto before = fdinfo::capture(bdf);
 *   std::this_thread::sleep_for(interval);
 *   auto after  = fdinfo::capture(bdf);
 *   auto utils  = fdinfo::delta(before, after);          // pid -> eng -> %
 *   proc.util   = fdinfo::toProcUtil(utils[proc.pid]);
 *
 * On non-Linux platforms all functions are stubs that return empty / zero values.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Per-process GPU utilisation across the four engine categories.
// nullopt means no data is available for that engine type.
struct ProcUtil
{
	std::optional<float> compute; ///< Compute Command Streamer  (ccs / compute)
	std::optional<float> copy;	  ///< Blitter / DMA             (bcs / copy)
	std::optional<float> render;  ///< Render Command Streamer   (rcs / render)
	std::optional<float> media;	  ///< Video engines             (vcs / vecs / video)
	// EU metrics — populated via L0 ZET proportional attribution (root / CAP_PERFMON only)
	std::optional<float> euActive; ///< EU active %  (shaders executing)
	std::optional<float> euStall;  ///< EU stall %   (shaders blocked — memory / barrier / etc.)
};

namespace fdinfo {

// Raw per-engine counters from one fdinfo file read.
struct EngineCounters
{
	uint64_t cycles = 0;	  ///< busy cycles (xe) or busy ns (i915)
	uint64_t totalCycles = 0; ///< total GPU cycles (xe); 0 → use wall-clock (i915)
	uint32_t capacity = 0;	  ///< drm-engine-capacity-<eng>; 0 when not reported, meaning one engine
};

// All engine counters for one process on one device at one point in time.
struct ProcessSnapshot
{
	uint32_t pid = 0;
	uint64_t wallNs = 0;									 ///< CLOCK_MONOTONIC
	std::unordered_map<std::string, EngineCounters> engines; ///< engine_name → counters
};

/// Per-engine utilisation map for a single process: engine_name → 0-100 %
using EngineUtilMap = std::unordered_map<std::string, float>;

/// Per-process engine utilisation map: pid → EngineUtilMap
using PidUtilMap = std::unordered_map<uint32_t, EngineUtilMap>;

/// Snapshot engine counters for every process whose fdinfo matches @p pciAddr.
/// @p procRoot defaults to "/proc"; override in tests to point at a fixture directory.
/// Returns empty vector on non-Linux platforms.
std::vector<ProcessSnapshot> capture(const std::string &pciAddr, const std::string &procRoot = "/proc");

/// Compute per-process, per-engine utilisation % from two snapshots.
/// Returns: pid → (engine_name → 0-100 %)
/// Returns empty map on non-Linux platforms.
PidUtilMap delta(const std::vector<ProcessSnapshot> &before, const std::vector<ProcessSnapshot> &after);

/// Map xe/i915 engine names to the four ProcUtil categories.
/// Takes the maximum across engines that fall in the same category.
ProcUtil toProcUtil(const EngineUtilMap &engineUtils);

/// Derive a zes_engine_type_flags_t bitmask from the engine keys present in a
/// snapshot.  The presence of a key (e.g. "rcs") indicates the process holds a
/// context on that engine even if its cycle count is currently zero.
/// Use this to fill psInfo::engines for processes where L0 reports engines == 0.
uint64_t enginesFromSnapshot(const ProcessSnapshot &snap);

/// Compute device-level engine utilization from two snapshots by summing per-process
/// utilization across all active PIDs, capping each engine type at 100%.
/// Equivalent to delta() + toProcUtil() per process + summing across all processes.
/// Only compute/render/media/copy fields are set; euActive/euStall remain nullopt.
/// Returns all-nullopt on non-Linux platforms.
ProcUtil aggregateDeviceUtil(const std::vector<ProcessSnapshot> &before, const std::vector<ProcessSnapshot> &after);

/**
 * @brief Whether engine utilization for a device should come from fdinfo rather than from
 *        the sysman engine counters
 *
 * The platform's policy, not just availability: the per-platform implementations say why.
 *
 * @param[in] pciAddr  PCI address of the device, e.g. "0000:03:00.0".
 * @param[in] procRoot Defaults to "/proc"; override in tests to point at a fixture directory.
 * @retval true  read engine utilization from fdinfo for this device
 * @retval false use the sysman engine counters
 */
[[nodiscard]] bool preferForEngineUtil(const std::string &pciAddr, const std::string &procRoot = "/proc");

/**
 * @brief Number of engines of each class on a device, from this process's own DRM fdinfo
 *
 * A class listed with drm-engine-capacity-<class> has that many engines; one listed without
 * it has one. Read from this process's DRM client entries only, so it needs neither engine
 * enumeration nor any other process's fdinfo.
 *
 * @param[in] pciAddr  PCI address of the device, e.g. "0000:03:00.0".
 * @param[in] procRoot Defaults to "/proc"; override in tests to point at a fixture directory.
 * @retval map   engine class name as in fdinfo (e.g. "ccs", "vcs") -> number of engines
 * @retval empty this process holds no DRM client of the device, or the platform has no fdinfo
 */
[[nodiscard]] std::unordered_map<std::string, uint32_t> engineCountsPerClass(const std::string &pciAddr,
																			 const std::string &procRoot = "/proc");

} // namespace fdinfo
