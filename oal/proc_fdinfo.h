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
#include <string_view>
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

/// Map a raw fdinfo engine key suffix ("ccs0", "ccs", "render/0") to its
/// zes_engine_type_flags_t bits.  Returns 0 for a name matching no reported class.
[[nodiscard]] uint64_t engineFlagForKey(std::string_view rawEngine);

/// Which per-engine counter an fdinfo key carries.
enum class EngineField : uint8_t
{
	Busy,	  ///< drm-cycles-<eng> (xe, cycles) or drm-engine-<eng> (i915, ns)
	Total,	  ///< drm-total-cycles-<eng> (xe): the denominator for Busy
	Capacity, ///< drm-engine-capacity-<eng>: engine count, not client state
};

/// An fdinfo key recognised as a per-engine counter.
struct EngineKey
{
	EngineField field;
	std::string engine; ///< normalised class token, e.g. "rcs", "ccs", "vcs"
};

/**
 * @brief Classifies an fdinfo key as a per-engine counter.
 *
 * The one place that knows the per-engine key spellings, so the parsers in
 * proc_fdinfo.cpp and process_platform.cpp cannot disagree about them.
 * drm-engine-capacity- shares its prefix with i915's drm-engine-<eng> busy key;
 * reading it as Busy would make every client look as if it had run work.
 *
 * @param[in] key  Text before the ':' on one fdinfo line, e.g. "drm-cycles-ccs0".
 *
 * @retval EngineKey     The key names a per-engine counter.
 * @retval std::nullopt  Any other key, or an engine counter with no engine name.
 */
[[nodiscard]] std::optional<EngineKey> parseEngineKey(std::string_view key);

/// Bitmask of the engines a snapshot shows non-zero cycles for.
///
/// Checks the cycle value, not whether the key is there.  xe lists every engine
/// the hardware has for every client, so a bare open() of the render node already
/// shows rcs/ccs/bcs/vcs/vecs at zero.  Matching on presence alone made every
/// process holding a DRM fd look like a compute process.
///
/// Cycle counts only ever rise, so a real workload still shows up while idle.
///
/// Use this to fill psInfo::engines for processes where L0 reports engines == 0.
[[nodiscard]] uint64_t enginesFromSnapshot(const ProcessSnapshot &snap);

/// Compute device-level engine utilization from two snapshots by summing per-process
/// utilization across all active PIDs, capping each engine type at 100%.
/// Equivalent to delta() + toProcUtil() per process + summing across all processes.
/// Only compute/render/media/copy fields are set; euActive/euStall remain nullopt.
/// Returns all-nullopt on non-Linux platforms.
ProcUtil aggregateDeviceUtil(const std::vector<ProcessSnapshot> &before, const std::vector<ProcessSnapshot> &after);

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
