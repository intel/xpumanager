/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "process_platform.h"
#include "proc_fdinfo.h"
#include <debug.h>
#include <algorithm>
#include <filesystem>
#include "utility/compat/format.h"
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
namespace fs = std::filesystem;

using namespace std::string_view_literals;

constexpr std::string_view CLIENT_ID_PREFIX = "drm-client-id:"sv;
constexpr std::string_view MEM_TOTAL_PREFIX = "drm-total-"sv;
constexpr uint64_t KIB_TO_BYTES = 1024;

enum class ParseState : uint8_t
{
	NoDeviceFd,	  // no fd in /proc/<pid>/fd pointed at a device node we own
	DeviceFdOnly, // found a device fd but parseFdinfo failed for every one
	GotClientId,  // found a device fd AND successfully parsed a drm-client-id
};

// drm-total-local* and drm-total-vram* name device-local VRAM (physical GPU
// memory).  Everything else (drm-total-system*, drm-total-gtt*, …) is
// GTT: system RAM mapped into the GPU's virtual address space.  On dGPUs we
// report only local VRAM so that GTT-backed allocations such as OA ring
// buffers do not inflate the process memory figure.  On iGPUs there is no
// local region, so we fall back to the total to preserve existing behaviour.
bool isLocalRegion(std::string_view fieldName)
{
	return fieldName.starts_with("local"sv) || fieldName.starts_with("vram"sv);
}

struct FdinfoEntry
{
	uint64_t clientId{0};
	uint64_t localKiB{0};		   // drm-total-local* / drm-total-vram* (physical VRAM)
	uint64_t totalKiB{0};		   // all drm-total-* regions combined
	uint64_t engineFlags{0};	   // engines with a non-zero busy counter
	bool sawEngineCounters{false}; // driver exported any engine counter at all
};

/**
 * @brief Reads the KiB/ns/cycle value following the "key:" separator on one fdinfo line.
 *
 * @param[in] line   One line of a /proc/<pid>/fdinfo/<fd> file.
 * @param[in] colon  Offset of the ':' that ends the key in @p line.
 *
 * @retval value         The leading unsigned integer after the separator; any unit
 *                       suffix ("KiB", "ns") is ignored.
 * @retval std::nullopt  The line has no value after the separator, or it does not
 *                       parse as an unsigned integer.
 */
[[nodiscard]] std::optional<uint64_t> fieldValue(std::string_view line, size_t colon)
{
	const size_t pos = line.find_first_not_of(" \t", colon + 1);
	if (pos == std::string_view::npos) {
		return std::nullopt;
	}
	try {
		// stoull stops at the unit suffix i915 appends ("12345 ns").
		return std::stoull(std::string{line.substr(pos)});
	} catch (...) {
		return std::nullopt;
	}
}

/**
 * @brief Records the engine of one busy-counter line (fdinfo::EngineField::Busy),
 *        but only when its counter is non-zero.
 *
 * Both drivers list every engine the hardware has for every client, so the key
 * being there proves nothing: a bare open() of the render node already shows
 * rcs/ccs/bcs/vcs/vecs at zero.  Cycle counts only ever rise, so a real workload
 * still counts while idle.
 *
 * @param[in]     value   The line's counter, or nullopt when it did not parse.
 * @param[in]     engine  Normalised engine class from fdinfo::parseEngineKey().
 * @param[in,out] entry   sawEngineCounters is set for any parseable value;
 *                        engineFlags gains the engine's bits when that value is
 *                        non-zero.  An unparseable value leaves it untouched.
 */
void accumulateEngineFlag(std::optional<uint64_t> value, std::string_view engine, FdinfoEntry &entry)
{
	if (!value) {
		return;
	}
	// Noting the line even at zero is what lets a caller tell "ran nothing" from
	// "driver reports no activity", which must not be treated the same way.
	entry.sawEngineCounters = true;
	if (*value == 0) {
		return;
	}
	// An engine class this build does not recognise still proves the client ran
	// work here, so record it as OTHER.  Leaving the mask empty would make a
	// process busy on a newly added class look idle and get it filtered out.
	const uint64_t flag = fdinfo::engineFlagForKey(engine);
	entry.engineFlags |= flag != 0 ? flag : static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_OTHER);
}

// Parses one drm-total-* line and accumulates its KiB value into entry.
// Returns false if the line should be skipped (cycles counter or parse error).
bool accumulateTotalMemLine(std::string_view line, FdinfoEntry &entry)
{
	const std::string_view suffix = line.substr(MEM_TOTAL_PREFIX.size());
	if (suffix.starts_with("cycles"sv)) {
		return false; // GPU utilisation counter, not memory
	}
	const size_t colon = line.find(':');
	if (colon == std::string::npos) {
		return false;
	}
	const auto kib = fieldValue(line, colon);
	if (!kib) {
		return false;
	}
	const std::string_view fieldName = line.substr(MEM_TOTAL_PREFIX.size(), colon - MEM_TOTAL_PREFIX.size());
	entry.totalKiB += *kib;
	if (isLocalRegion(fieldName)) {
		entry.localKiB += *kib;
	}
	return true;
}

/** Parses a single /proc/<pid>/fdinfo/<fd> file for drm-client-id and all drm-total-* fields.
 *  Returns nullopt if the file could not be opened or contained no drm-client-id line. */
std::optional<FdinfoEntry> parseFdinfo(const fs::path &path)
{
	std::ifstream fin(path);
	if (!fin) {
		return std::nullopt;
	}
	FdinfoEntry entry;
	bool hasClientId = false;
	std::string line;
	while (std::getline(fin, line)) {
		if (line.starts_with(CLIENT_ID_PREFIX)) {
			try {
				entry.clientId = std::stoull(line.substr(CLIENT_ID_PREFIX.size()));
				hasClientId = true;
			} catch (...) {
			}
		} else if (line.starts_with(MEM_TOTAL_PREFIX)) {
			accumulateTotalMemLine(line, entry);
		} else if (const size_t colon = line.find(':'); colon != std::string::npos) {
			const auto ek = fdinfo::parseEngineKey(std::string_view{line}.substr(0, colon));
			if (ek && ek->field == fdinfo::EngineField::Busy) {
				accumulateEngineFlag(fieldValue(line, colon), ek->engine, entry);
			}
		}
	}
	return hasClientId ? std::optional{entry} : std::nullopt;
}

} // namespace

std::vector<std::string> deviceNodesForBdf(const std::string &bdf, const fs::path &sysRoot)
{
	std::vector<std::string> nodes;
	std::error_code ec;
	for (const auto &entry : fs::directory_iterator(sysRoot, ec)) {
		if (ec) {
			break;
		}
		const std::string name = entry.path().filename().string();
		if (name.rfind("card", 0) != 0 || name.find('-') != std::string::npos) {
			continue;
		}
		auto resolved = fs::canonical(entry.path(), ec);
		if (ec || resolved.string().find(bdf) == std::string::npos) {
			ec.clear();
			continue;
		}
		const fs::path devDrmDir = entry.path() / "device" / "drm";
		if (fs::is_directory(devDrmDir, ec)) {
			for (const auto &node : fs::directory_iterator(devDrmDir, ec)) {
				const std::string nodeName = node.path().filename().string();
				if (nodeName.rfind("card", 0) == 0 || nodeName.rfind("renderD", 0) == 0) {
					nodes.push_back("/dev/dri/" + nodeName);
				}
			}
		}
		break;
	}
	return nodes;
}

FdinfoResult vramFromFdinfo(uint32_t pid, const std::vector<std::string> &deviceNodes, const std::string &procRoot,
							std::unordered_set<uint64_t> *globalSeenIds)
{
	if (deviceNodes.empty()) {
		return {};
	}

	std::unordered_map<uint64_t, FdinfoEntry> clientMem;
	ParseState parseState = ParseState::NoDeviceFd;
	uint64_t engineFlags = 0;
	bool engineCountersPresent = false;
	std::error_code ec;

	const fs::path fdDir = fs::path(procRoot) / std::to_string(pid) / "fd";
	// Accessible = the fd directory exists and we could open it, regardless of
	// whether it contains any GPU-node symlinks.  An openable-but-empty fd dir
	// means the process is real and we can trust fdinfo (bytes=0); an
	// inaccessible dir (hidepid=2, cross-user ENOENT/EACCES) means we must fall
	// back to Level Zero's reported value.
	std::error_code openEc;
	fs::directory_iterator fdIter(fdDir, openEc);
	const bool fdDirAccessible = !openEc;
	for (; fdIter != fs::directory_iterator{}; fdIter.increment(ec)) {
		if (ec) {
			break;
		}
		const auto &fdEntry = *fdIter;

		fs::path target = fs::read_symlink(fdEntry.path(), ec);
		if (ec) {
			ec.clear();
			continue;
		}

		const std::string targetStr = target.string();
		if (!std::ranges::any_of(deviceNodes, [&](const auto &n) { return targetStr == n; })) {
			continue;
		}
		if (parseState == ParseState::NoDeviceFd) {
			parseState = ParseState::DeviceFdOnly;
		}

		const std::string fdNum = fdEntry.path().filename().string();
		const fs::path fdinfoPath = fs::path(procRoot) / std::to_string(pid) / "fdinfo" / fdNum;
		const auto entry = parseFdinfo(fdinfoPath);
		if (!entry) {
			continue;
		}
		parseState = ParseState::GotClientId;
		// Accumulated before the dedup check below: sharing a drm-client-id with
		// another process changes who the memory is attributed to, not whether work
		// actually ran on this device.
		engineFlags |= entry->engineFlags;
		engineCountersPresent = engineCountersPresent || entry->sawEngineCounters;
		if (globalSeenIds != nullptr && globalSeenIds->count(entry->clientId) != 0) {
			continue;
		}

		auto [it, inserted] = clientMem.emplace(entry->clientId, *entry);
		if (!inserted && entry->totalKiB > it->second.totalKiB) {
			it->second = *entry;
		}
	}

	uint64_t localKiB = 0;
	uint64_t totalKiB = 0;
	for (const auto &[id, mem] : clientMem) {
		localKiB += mem.localKiB;
		totalKiB += mem.totalKiB;
		if (globalSeenIds != nullptr) {
			globalSeenIds->insert(id);
		}
	}
	const bool allInherited = (parseState == ParseState::GotClientId) && clientMem.empty();
	const bool fdinfoReliable = parseState != ParseState::DeviceFdOnly;
	return {.localBytes = localKiB * KIB_TO_BYTES,
			.totalBytes = totalKiB * KIB_TO_BYTES,
			.allInherited = allInherited,
			.fdDirAccessible = fdDirAccessible,
			.fdinfoReliable = fdinfoReliable,
			.engineFlags = engineFlags,
			.engineCountersPresent = engineCountersPresent};
}

/**
 * @brief Rewrites memSize and engines for every entry in @p processList from
 *        /proc/<pid>/fdinfo, and drops processes that are not clients of the device.
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
 * Also narrows the list to processes that are real clients of this device, and
 * rewrites their engines mask to match.  A process is dropped when fdinfo exported
 * engine counters and every one of them reads zero, i.e. it has never executed
 * work here: zeInit() opens a DRM fd and makes small internal allocations on every
 * GPU in the system, so holding VRAM on a device does not make a process a client
 * of it.
 *
 * Both halves of that condition matter.  A process whose fdinfo could not be read
 * (hidepid, cross-user), and every process on a driver that exports no
 * drm-cycles-<eng> fields, has engineFlags == 0 for want of data rather than want
 * of activity; those are kept, since dropping them would report no processes at
 * all.  See FdinfoResult::engineCountersPresent.
 *
 * Engine filtering needs only the DRM nodes and fdinfo, so it does not depend on
 * @p memKind being known.  Passing nullopt disables the memory rewrite alone and
 * still removes phantoms: the GPU type is only needed to choose between VRAM and
 * combined totals, and guessing it risks zeroing valid Level Zero values on a dGPU.
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
 *                             entries, and those that never ran work here, are
 *                             erased.
 * @param[in]     memKind      MemKind::Local  - device has dedicated VRAM (dGPU):
 *                             only drm-total-local/drm-total-vram bytes are
 *                             reported; GTT-only entries get memSize=0 so the
 *                             zero-memory filter removes cross-GPU phantoms.
 *                             MemKind::Shared - integrated GPU: all drm-total
 *                             regions are summed since there is no dedicated
 *                             local region.
 *                             nullopt - GPU type unknown: memSize is left exactly
 *                             as Level Zero reported it.
 *
 * @return PIDs whose engines mask this call rewrote from fdinfo counters.  Empty
 *         when no DRM node matched @p bdf.
 */
std::unordered_set<uint32_t> applyFdinfoCorrections(const std::string &bdf,
													std::vector<zes_process_state_t> *processList,
													std::optional<MemKind> memKind)
{
	std::unordered_set<uint32_t> engineVerified;

	const std::vector<std::string> nodes = deviceNodesForBdf(bdf);
	if (nodes.empty()) {
		ERR("No DRM nodes found in sysfs for device {} — process memory reporting unavailable\n", bdf);
		return engineVerified;
	}

	// Sort ascending by PID so parents (lower PID) are processed before their
	// forked children; this ensures the shared drm-client-id is attributed to
	// the parent and children marked allInherited are correctly removed.
	std::ranges::sort(*processList, {}, &zes_process_state_t::processId);

	std::unordered_set<uint64_t> globalSeenIds;
	std::vector<FdinfoResult> results;
	results.reserve(processList->size());
	std::ranges::transform(*processList, std::back_inserter(results), [&](const zes_process_state_t &ps) {
		return vramFromFdinfo(ps.processId, nodes, "/proc", &globalSeenIds);
	});

	size_t i = 0;
	for (auto it = processList->begin(); it != processList->end();) {
		const FdinfoResult &res = results[i++];
		if (res.allInherited) {
			it = processList->erase(it);
			continue;
		}

		// Only fdinfo can say whether work ever ran on *this* device, so the engine
		// rule needs both a readable /proc entry and a driver that exports the
		// counters.  Without either, engineFlags reads 0 for every process -- under
		// hidepid, for another user's process, or on a driver with no drm-cycles-*
		// fields at all -- and acting on that would empty the list, so keep Level
		// Zero's view instead.
		const bool fdinfoTrusted = res.fdDirAccessible && res.fdinfoReliable;
		const bool haveEngineData = fdinfoTrusted && res.engineCountersPresent;
		if (haveEngineData && res.engineFlags == 0) {
			// zeInit() opens a DRM fd on every GPU and makes small internal
			// allocations there, so holding VRAM does not make a process a client of
			// this device -- having executed work on it does.
			DBG("  - PID {} filtered: holds memory on {} but has never run work there\n", it->processId, bdf);
			it = processList->erase(it);
			continue;
		}
		if (haveEngineData) {
			// Level Zero reports the same engine mask on every device the process
			// zeInit'd; replace it with the engines this device actually saw, and
			// record the PID so the caller knows this mask is fdinfo-derived.
			it->engines = static_cast<zes_engine_type_flags_t>(res.engineFlags);
			engineVerified.insert(it->processId);
		}

		// Only the memory rewrite needs the GPU type, to choose between VRAM-only
		// and combined totals.  Without it, leave Level Zero's figure alone rather
		// than guess and risk zeroing a valid value on a dGPU.
		if (memKind) {
			// On dGPUs use only local VRAM bytes so GTT-only processes (which have
			// contexts open but no physical allocation here) get memSize=0 and are
			// removed by the caller's zero-memory filter.  On iGPUs there is no
			// local region, so fall back to the combined total.
			const uint64_t bytes = *memKind == MemKind::Local ? res.localBytes : res.totalBytes;
			if (bytes > 0 || fdinfoTrusted) {
				// When fdinfo was readable, trust it over Level Zero even when bytes
				// is zero: a readable fd directory with no device fds means the
				// process has no real allocation here and Level Zero's figure is
				// mis-attributed (e.g. parent PID credited for a child's memory).
				// When fdinfo was unreadable (hidepid, cross-user), bytes is also 0
				// but fdDirAccessible is false, so Level Zero's value is preserved.
				it->memSize = bytes;
			}
		}
		++it;
	}
	return engineVerified;
}
