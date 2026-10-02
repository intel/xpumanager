/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef OAL_PROCESS_PLATFORM_H
#define OAL_PROCESS_PLATFORM_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>
#include <zes_api.h>

/**
 * @brief Returns /dev/dri/{card*,renderD*} device node paths for the given PCI BDF.
 *
 * Walks @p sysRoot to find the card<N> entry whose canonical path contains
 * @p bdf, then enumerates its device/drm/ subdirectory to collect all associated
 * DRM node names.  Returns an empty vector if @p bdf is not found or on error.
 *
 * @param bdf      PCI BDF string, e.g. "0000:4d:00.0".
 * @param sysRoot  Root of the DRM sysfs class directory; override in unit tests.
 */
[[nodiscard]] std::vector<std::string> deviceNodesForBdf(const std::string &bdf,
														 const std::filesystem::path &sysRoot = "/sys/class/drm");

/**
 * @brief Result returned by vramFromFdinfo.
 *
 * localBytes      - Bytes from drm-total-local* / drm-total-vram* regions only
 *                   (physical VRAM on dGPUs).  Zero on iGPUs and for processes
 *                   that have only GTT/system allocations on this device.
 * totalBytes      - Bytes from all drm-total-* regions combined (VRAM + GTT).
 * allInherited    - true when the process had at least one valid drm-client-id but
 *                   every one of them was already present in the globalSeenIds set
 *                   (i.e. all GPU contexts were inherited from another process via
 *                   fork and should not be attributed here again).  Always false
 *                   when globalSeenIds is null or the process has no DRM fds.
 * fdDirAccessible - true when /proc/<pid>/fd was successfully iterated.  When
 *                   false the fd directory was unreadable (hidepid, cross-user,
 *                   or the process no longer exists) and Level Zero's memSize
 *                   should be preserved as a fallback.  When true but both byte
 *                   fields are zero the process has no allocation on this device
 *                   and Level Zero's figure is mis-attributed (e.g. a parent PID
 *                   credited for memory actually owned by a child process).
 * engineFlags     - zes_engine_type_flags_t bits for the engines this process has
 *                   actually executed work on, from drm-cycles-<eng> counters with
 *                   a non-zero value.  Zero means the process has never run work
 *                   on this device, which is how a zeInit-only phantom is told
 *                   apart from a real client: zeInit opens a DRM fd (and small
 *                   internal allocations) on every GPU, but submits nothing.  The
 *                   counters only ever rise, so this stays set for a real workload
 *                   that is idle when sampled.  Only meaningful when
 *                   engineCountersPresent is true.
 *
 *                   A non-zero counter for an engine class this build does not
 *                   recognise contributes ZES_ENGINE_TYPE_FLAG_OTHER, so the mask
 *                   is non-zero whenever any work ran.  Without that a process
 *                   busy on a newly added class would look idle and be dropped.
 * engineCountersPresent
 *                 - true when fdinfo exported at least one drm-cycles-<eng> or
 *                   drm-engine-<eng> field, whatever its value.  Distinguishes
 *                   "ran nothing" from "driver reports no activity": a driver that
 *                   omits those fields leaves engineFlags at 0 for every process,
 *                   and filtering on that would report no processes at all.
 */
struct FdinfoResult
{
	uint64_t localBytes{0};
	uint64_t totalBytes{0};
	bool allInherited{false};
	bool fdDirAccessible{false};
	bool fdinfoReliable{false};
	uint64_t engineFlags{0};
	bool engineCountersPresent{false};
};

/**
 * @brief Returns VRAM attributed to a process by parsing /proc/<pid>/fdinfo.
 *
 * Walks /proc/@p pid/fd, resolves each symlink against @p deviceNodes, then
 * reads the matching fdinfo file to extract drm-client-id and the sum of all
 * drm-total-<region> fields (drm-total-vram0 on dGPUs, drm-total-local0 /
 * drm-total-system on iGPUs; future region names are handled automatically).
 * Entries sharing a drm-client-id are counted only once (takes the maximum
 * reported value across fds for the same id).
 *
 * @param[in]     pid           Process ID to inspect.
 * @param[in]     deviceNodes   /dev/dri/{card*,renderD*} paths for the target GPU.
 * @param[in]     procRoot      Root of the procfs tree; defaults to "/proc".
 *                              Override in unit tests to point at a temp directory.
 * @param[in,out] globalSeenIds When non-null, client-ids already in the set are
 *                              skipped; newly counted ids are inserted before
 *                              returning.  Pass the same set across multiple PIDs
 *                              to avoid double-counting contexts inherited via fork.
 *
 * @retval FdinfoResult::localBytes      Physical VRAM bytes from drm-total-local and drm-total-vram regions.
 * @retval FdinfoResult::totalBytes      All drm-total regions combined (VRAM + GTT).
 * @retval FdinfoResult::allInherited    true when all DRM contexts were inherited.
 * @retval FdinfoResult::fdDirAccessible true when /proc/<pid>/fd was iterable.
 */
[[nodiscard]] FdinfoResult vramFromFdinfo(uint32_t pid, const std::vector<std::string> &deviceNodes,
										  const std::string &procRoot = "/proc",
										  std::unordered_set<uint64_t> *globalSeenIds = nullptr);

/**
 * @brief Memory class a device allocates from, which decides which of a process's
 *        allocations count toward its memSize.
 */
enum class MemKind : uint8_t
{
	Shared, ///< Integrated GPU: no device-local memory, so shared allocations count.
	Local	///< Discrete GPU: only device-local VRAM counts.
};

/**
 * @brief Corrects @p processList from the OS's own per-process GPU accounting.
 *
 * Level Zero can attribute memory and engines to processes that are not really
 * using this device.  Where the OS keeps per-process accounting of its own, this
 * rewrites memSize and engines from it and erases entries that are not real
 * clients of the device, leaving the caller to apply its own filter on top.
 *
 * On Linux:   reads /proc/<pid>/fdinfo; see oal/lin/process_platform.cpp.
 * On Windows: no-op; see oal/win/process_platform.cpp.
 *
 * @pre  @p processList must not be null.
 *
 * @param[in]     bdf          PCI BDF string for the device, e.g. "0000:4d:00.0".
 * @param[in,out] processList  Entries corrected in place; those that are not real
 *                             clients of the device are erased.
 * @param[in]     memKind      Memory class of the device, or nullopt when it is
 *                             unknown, in which case memSize is left exactly as
 *                             Level Zero reported it.
 *
 * @return PIDs whose engines mask this call replaced with verified activity.  Only
 *         for those is a bare ZES_ENGINE_TYPE_FLAG_OTHER evidence of work; on any
 *         other entry the mask is still the driver's, where OTHER alone means no
 *         real workload (XPUM-1242).
 */
[[nodiscard]] std::unordered_set<uint32_t> applyFdinfoCorrections(const std::string &bdf,
																  std::vector<zes_process_state_t> *processList,
																  std::optional<MemKind> memKind);

#endif // OAL_PROCESS_PLATFORM_H
