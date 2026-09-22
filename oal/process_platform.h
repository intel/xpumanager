/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef OAL_PROCESS_PLATFORM_H
#define OAL_PROCESS_PLATFORM_H

#include <cstdint>
#include <filesystem>
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
 */
struct FdinfoResult
{
	uint64_t localBytes{0};
	uint64_t totalBytes{0};
	bool allInherited{false};
	bool fdDirAccessible{false};
	bool fdinfoReliable{false};
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
 * @brief Rewrites memSize for every entry in @p processList using fdinfo accounting.
 *
 * zesDeviceProcessesGetState() accumulates GPU memory from every open fd
 * without deduplicating by drm-client-id, so a process with N fds to the same
 * GPU context is over-reported by a factor of N.  This function corrects memSize
 * by reading /proc/<pid>/fdinfo directly and counting each drm-client-id once.
 * A single globalSeenIds set is shared across all PIDs so contexts inherited via
 * fork are attributed to the first process in the list and not double-counted.
 * Processes whose every drm-client-id was already attributed to another process
 * (allInherited == true) are removed from processList entirely.
 *
 * @pre  @p processList must not be null.
 *
 * @param[in]     bdf          PCI BDF string for the device, e.g. "0000:4d:00.0".
 *                             Used to locate the matching DRM nodes under sysfs.
 * @param[in,out] processList  Process entries whose memSize fields are rewritten
 *                             in-place when fdinfo yields a non-zero value;
 *                             the Level Zero-reported memSize is preserved as a
 *                             fallback when /proc/<pid>/fd is unreadable (e.g.
 *                             hidepid or cross-user processes).  Fully-inherited
 *                             entries are erased.  On Windows this is a no-op.
 * @param[in]     memKind      MemKind::Local  — device has dedicated VRAM (dGPU):
 *                             only drm-total-local/drm-total-vram bytes are
 *                             reported; GTT-only entries get memSize=0 so the
 *                             zero-memory filter removes cross-GPU phantoms.
 *                             MemKind::Shared — integrated GPU: all drm-total
 *                             regions are summed since there is no dedicated
 *                             local region.
 */
enum class MemKind : uint8_t
{
	Shared,
	Local
};

void fixProcessMemSize(const std::string &bdf, std::vector<zes_process_state_t> *processList,
					   MemKind memKind = MemKind::Shared);

#endif // OAL_PROCESS_PLATFORM_H
