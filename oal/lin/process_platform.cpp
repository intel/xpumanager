/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "process_platform.h"
#include <debug.h>
#include <algorithm>
#include <filesystem>
#include <format>
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
// Sum every drm-total-<region> field: covers drm-total-vram0 (dGPU),
// drm-total-local0 (iGPU device-local / stolen memory), drm-total-system
// (GTT/shared), and any future region names the driver may add.
constexpr std::string_view MEM_TOTAL_PREFIX = "drm-total-"sv;
constexpr uint64_t KIB_TO_BYTES = 1024;

struct FdinfoEntry
{
	uint64_t clientId{0};
	uint64_t memKiB{0};
};

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
			// drm-total-cycles-<engine> are GPU utilisation counters (raw cycles,
			// no unit), not memory.  Skip them; every other drm-total-* field is
			// a memory region reported in KiB.
			if (std::string_view{line}.substr(MEM_TOTAL_PREFIX.size()).starts_with("cycles"sv)) {
				continue;
			}
			// "drm-total-gtt:\t1101108 KiB", "drm-total-vram0:\t1234 KiB", etc.
			const size_t colon = line.find(':');
			if (colon != std::string::npos) {
				const size_t pos = line.find_first_not_of(" \t", colon + 1);
				if (pos != std::string::npos) {
					try {
						entry.memKiB += std::stoull(line.substr(pos));
					} catch (...) {
					}
				}
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

	std::unordered_map<uint64_t, uint64_t> clientVramKiB;
	bool foundAnyClientId = false;
	std::error_code ec;

	const fs::path fdDir = fs::path(procRoot) / std::to_string(pid) / "fd";
	for (const auto &fdEntry : fs::directory_iterator(fdDir, fs::directory_options::skip_permission_denied, ec)) {
		if (ec) {
			break;
		}

		fs::path target = fs::read_symlink(fdEntry.path(), ec);
		if (ec) {
			ec.clear();
			continue;
		}

		const std::string targetStr = target.string();
		if (!std::ranges::any_of(deviceNodes, [&](const auto &n) { return targetStr == n; })) {
			continue;
		}

		const std::string fdNum = fdEntry.path().filename().string();
		const fs::path fdinfoPath = fs::path(procRoot) / std::to_string(pid) / "fdinfo" / fdNum;
		const auto entry = parseFdinfo(fdinfoPath);
		if (!entry) {
			continue;
		}
		foundAnyClientId = true;
		if (globalSeenIds != nullptr && globalSeenIds->count(entry->clientId) != 0) {
			continue;
		}

		auto [it, inserted] = clientVramKiB.emplace(entry->clientId, entry->memKiB);
		if (!inserted && entry->memKiB > it->second) {
			it->second = entry->memKiB;
		}
	}

	uint64_t totalKiB = 0;
	for (const auto &[id, kib] : clientVramKiB) {
		totalKiB += kib;
		if (globalSeenIds != nullptr) {
			globalSeenIds->insert(id);
		}
	}
	const bool allInherited = foundAnyClientId && clientVramKiB.empty();
	return {totalKiB * KIB_TO_BYTES, allInherited};
}

void fixProcessMemSize(const std::string &bdf, std::vector<zes_process_state_t> *processList)
{
	const std::vector<std::string> nodes = deviceNodesForBdf(bdf);
	if (nodes.empty()) {
		ERR("No DRM nodes found in sysfs for device {} — process memory reporting unavailable\n", bdf);
		return;
	}

	// Sort ascending by PID so parents (lower PID) are processed before their
	// forked children; this ensures the shared drm-client-id is attributed to
	// the parent and children marked allInherited are correctly removed.
	std::sort(processList->begin(), processList->end(),
			  [](const zes_process_state_t &a, const zes_process_state_t &b) { return a.processId < b.processId; });

	std::unordered_set<uint64_t> globalSeenIds;
	for (auto it = processList->begin(); it != processList->end();) {
		const FdinfoResult res = vramFromFdinfo(it->processId, nodes, "/proc", &globalSeenIds);
		if (res.allInherited) {
			it = processList->erase(it);
		} else {
			if (res.bytes > 0) {
				it->memSize = res.bytes;
			}
			++it;
		}
	}
}
