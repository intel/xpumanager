/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "process_platform.h"
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
	uint64_t localKiB{0}; // drm-total-local* / drm-total-vram* (physical VRAM)
	uint64_t totalKiB{0}; // all drm-total-* regions combined
};

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
	const size_t pos = line.find_first_not_of(" \t", colon + 1);
	if (pos == std::string::npos) {
		return false;
	}
	try {
		const uint64_t kib = std::stoull(std::string{line.substr(pos)});
		const std::string_view fieldName = line.substr(MEM_TOTAL_PREFIX.size(), colon - MEM_TOTAL_PREFIX.size());
		entry.totalKiB += kib;
		if (isLocalRegion(fieldName)) {
			entry.localKiB += kib;
		}
		return true;
	} catch (...) {
		return false;
	}
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
	return {localKiB * KIB_TO_BYTES, totalKiB * KIB_TO_BYTES, allInherited, fdDirAccessible, fdinfoReliable};
}

void fixProcessMemSize(const std::string &bdf, std::vector<zes_process_state_t> *processList, MemKind memKind)
{
	const std::vector<std::string> nodes = deviceNodesForBdf(bdf);
	if (nodes.empty()) {
		ERR("No DRM nodes found in sysfs for device {} — process memory reporting unavailable\n", bdf);
		return;
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
		} else {
			// On dGPUs use only local VRAM bytes so GTT-only processes (which have
			// contexts open but no physical allocation here) get memSize=0 and are
			// removed by the caller's zero-memory filter.  On iGPUs there is no
			// local region, so fall back to the combined total.
			const uint64_t bytes = memKind == MemKind::Local ? res.localBytes : res.totalBytes;
			if (bytes > 0 || (res.fdDirAccessible && res.fdinfoReliable)) {
				// When fdinfo was readable, trust it over Level Zero even when bytes
				// is zero: a readable fd directory with no device fds means the
				// process has no real allocation here and Level Zero's figure is
				// mis-attributed (e.g. parent PID credited for a child's memory).
				// When fdinfo was unreadable (hidepid, cross-user), bytes is also 0
				// but fdDirAccessible is false, so Level Zero's value is preserved.
				it->memSize = bytes;
			}
			++it;
		}
	}
}
