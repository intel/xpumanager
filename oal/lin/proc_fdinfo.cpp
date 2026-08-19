/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "../proc_fdinfo.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <zes_api.h>

namespace fdinfo {

/* Internal helpers */

static constexpr uint64_t NS_PER_SEC = 1'000'000'000ULL;

static uint64_t monotonicNs() noexcept
{
	struct timespec ts
	{};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return static_cast<uint64_t>(ts.tv_sec) * NS_PER_SEC + static_cast<uint64_t>(ts.tv_nsec);
}

// Normalise i915 engine names to the same tokens the xe driver uses.
//   "render/0" → "rcs"   "copy/0" → "bcs"
//   "video/0"  → "vcs"   "video-enhance/0" → "vecs"   "compute/0" → "ccs"
static std::string normaliseI915(std::string_view raw)
{
	std::string name{raw};
	if (const auto p = name.rfind('/'); p != std::string::npos) {
		name.resize(p);
	}
	if (name == "render") {
		return "rcs";
	}
	if (name == "copy") {
		return "bcs";
	}
	if (name == "video") {
		return "vcs";
	}
	if (name == "video-enhance") {
		return "vecs";
	}
	if (name == "compute") {
		return "ccs";
	}
	return name;
}

// Parse one /proc/<pid>/fdinfo/<fd> file.
// Returns an empty map if the file is not a DRM entry for expectedPci.
static std::unordered_map<std::string, EngineCounters>
parseFdinfo(const std::string &path, // NOLINT(bugprone-easily-swappable-parameters)
			const std::string &expectedPci)
{
	std::ifstream fin{path};
	if (!fin) {
		return {};
	}

	static constexpr std::string_view drivKey = "drm-driver";
	static constexpr std::string_view pdevKey = "drm-pdev";
	static constexpr std::string_view cycPfx = "drm-cycles-";
	static constexpr std::string_view totPfx = "drm-total-cycles-";
	static constexpr std::string_view engPfx = "drm-engine-";
	static constexpr std::string_view capPfx = "drm-engine-capacity-";

	std::unordered_map<std::string, EngineCounters> engines;
	std::string pdev;
	bool isDrm = false;

	for (std::string line; std::getline(fin, line);) {
		// All DRM fdinfo fields follow "key:\tvalue" format.
		const auto sep = line.find(":\t");
		if (sep == std::string::npos) {
			continue;
		}

		const std::string_view key{line.data(), sep};
		std::string_view val = std::string_view{line}.substr(sep + 2);
		while (!val.empty() && (val.back() == '\r' || val.back() == '\n' || val.back() == ' ')) {
			val.remove_suffix(1);
		}

		const auto parseU64 = [&](std::string_view sv) noexcept {
			uint64_t v = 0;
			std::from_chars(sv.data(), sv.data() + sv.size(), v);
			return v;
		};

		if (key == drivKey) {
			isDrm = true;
		} else if (key == pdevKey) {
			pdev = std::string{val};
		} else if (key.starts_with(cycPfx)) {
			engines[std::string{key.substr(cycPfx.size())}].cycles = parseU64(val);
		} else if (key.starts_with(totPfx)) {
			engines[std::string{key.substr(totPfx.size())}].totalCycles = parseU64(val);
		} else if (key.starts_with(engPfx) && !key.starts_with(capPfx)) {
			// i915: "drm-engine-render/0:\t12345 ns"
			const std::string eng = normaliseI915(key.substr(engPfx.size()));
			engines[eng].cycles = parseU64(val);
			engines[eng].totalCycles = 0; // sentinel: use wall-clock delta
		}
	}

	if (!isDrm || pdev != expectedPci) {
		return {};
	}
	return engines;
}

/* Public API */

/* Scan all open file descriptors of @p pid for DRM fds belonging to @p pciAddr
 * and merge their engine counters into a ProcessSnapshot.
 * Returns the snapshot if at least one matching fd was found, else nullopt. */
static std::optional<ProcessSnapshot>
captureOnePid(uint32_t pid, const std::string &pciAddr, // NOLINT(bugprone-easily-swappable-parameters)
			  const std::string &procRoot, uint64_t wallNs)
{
	namespace fs = std::filesystem;

	const fs::path base = fs::path{procRoot} / std::to_string(pid);
	const fs::path fdDir = base / "fd";
	const fs::path fdinfoDir = base / "fdinfo";

	std::error_code ec;
	fs::directory_iterator fds{fdDir, ec};
	if (ec) {
		return std::nullopt;
	}

	ProcessSnapshot snap;
	snap.pid = pid;
	snap.wallNs = wallNs;

	for (const auto &fdEnt : fds) {
		const fs::path target = fs::read_symlink(fdEnt.path(), ec);
		if (ec) {
			ec.clear();
			continue;
		}
		if (target.parent_path() != "/dev/dri") {
			continue;
		}

		const fs::path fdinfoPath = fdinfoDir / fdEnt.path().filename();
		auto info = parseFdinfo(fdinfoPath.string(), pciAddr);
		if (info.empty()) {
			continue;
		}

		for (auto &[eng, cnt] : info) {
			auto &dst = snap.engines[eng];
			dst.cycles += cnt.cycles;
			dst.totalCycles = std::max(dst.totalCycles, cnt.totalCycles);
		}
	}

	return snap.engines.empty() ? std::nullopt : std::make_optional(std::move(snap));
}

std::vector<ProcessSnapshot> capture(const std::string &pciAddr, // NOLINT(bugprone-easily-swappable-parameters)
									 const std::string &procRoot)
{
	if (pciAddr.empty()) {
		return {};
	}

	namespace fs = std::filesystem;
	const uint64_t wallNs = monotonicNs();
	std::vector<ProcessSnapshot> result;

	const auto isNumericPid = [](std::string_view s) noexcept {
		return !s.empty() && s[0] >= '1' && std::ranges::all_of(s, [](char c) { return c >= '0' && c <= '9'; });
	};

	std::error_code ec;
	for (const auto &procEnt : fs::directory_iterator{procRoot, ec}) {
		const std::string name = procEnt.path().filename().string();
		if (!isNumericPid(name)) {
			continue;
		}

		uint32_t pid = 0;
		const std::string_view sv{name};
		std::from_chars(sv.begin(), sv.end(), pid);
		if (pid == 0) {
			continue;
		}

		if (auto snap = captureOnePid(pid, pciAddr, procRoot, wallNs)) {
			result.push_back(std::move(*snap));
		}
	}

	return result;
}

PidUtilMap delta(const std::vector<ProcessSnapshot> &before, // NOLINT(bugprone-easily-swappable-parameters)
				 const std::vector<ProcessSnapshot> &after)
{
	// Index the before snapshots by PID for O(1) lookup.
	std::unordered_map<uint32_t, const ProcessSnapshot *> bmap;
	bmap.reserve(before.size());
	for (const auto &s : before) {
		bmap[s.pid] = &s;
	}

	PidUtilMap result;

	for (const auto &snap : after) {
		const auto bit = bmap.find(snap.pid);
		if (bit == bmap.end()) {
			continue;
		}
		const ProcessSnapshot &bsnap = *bit->second;

		if (snap.wallNs <= bsnap.wallNs) {
			continue;
		}
		const uint64_t wallDelta = snap.wallNs - bsnap.wallNs;

		auto &utilMap = result[snap.pid];

		for (const auto &[eng, acnt] : snap.engines) {
			const auto bit2 = bsnap.engines.find(eng);
			if (bit2 == bsnap.engines.end()) {
				continue;
			}
			const EngineCounters &bcnt = bit2->second;

			if (acnt.cycles < bcnt.cycles) {
				continue;
			} // counter reset, skip

			const uint64_t dCycles = acnt.cycles - bcnt.cycles;

			float util = 0.0F;
			if (acnt.totalCycles > bcnt.totalCycles) {
				// xe: cycles-based utilisation
				const uint64_t dTotal = acnt.totalCycles - bcnt.totalCycles;
				util = std::clamp(static_cast<float>(dCycles) / static_cast<float>(dTotal) * 100.0F, 0.0F, 100.0F);
			} else {
				// i915: wall-clock-based utilisation (totalCycles == 0 sentinel)
				util = std::clamp(static_cast<float>(dCycles) / static_cast<float>(wallDelta) * 100.0F, 0.0F, 100.0F);
			}

			utilMap[eng] = util;
		}
	}

	return result;
}

uint64_t enginesFromSnapshot(const ProcessSnapshot &snap)
{
	uint64_t flags = 0;
	for (const auto &[name, _] : snap.engines) {
		if (name == "rcs" || name == "render") {
			flags |=
				static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_RENDER) | static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_3D);
		} else if (name == "ccs" || name == "compute") {
			flags |= static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_COMPUTE);
		} else if (name == "bcs" || name == "copy") {
			flags |= static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_DMA);
		} else if (name == "vcs" || name == "vecs" || name == "video" || name == "video-enhance") {
			flags |= static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_MEDIA);
		}
	}
	return flags;
}

ProcUtil toProcUtil(const EngineUtilMap &engineUtils)
{
	ProcUtil pu;

	const auto maybeMax = [](std::optional<float> &dest, float val) {
		dest = dest.has_value() ? std::max(*dest, val) : val;
	};

	for (const auto &[name, pct] : engineUtils) {
		if (name == "ccs" || name == "compute") {
			maybeMax(pu.compute, pct);
		} else if (name == "rcs" || name == "render") {
			maybeMax(pu.render, pct);
		} else if (name == "bcs" || name == "copy") {
			maybeMax(pu.copy, pct);
		} else if (name == "vcs" || name == "vecs" || name == "video" || name == "video-enhance") {
			maybeMax(pu.media, pct);
		}
	}

	return pu;
}

ProcUtil aggregateDeviceUtil(const std::vector<ProcessSnapshot> &before, const std::vector<ProcessSnapshot> &after)
{
	const auto utilMap = delta(before, after);

	ProcUtil agg;
	for (const auto &[pid, engMap] : utilMap) {
		const auto pu = toProcUtil(engMap);
		if (pu.compute) {
			agg.compute = std::make_optional(agg.compute.value_or(0.0F) + *pu.compute);
		}
		if (pu.render) {
			agg.render = std::make_optional(agg.render.value_or(0.0F) + *pu.render);
		}
		if (pu.media) {
			agg.media = std::make_optional(agg.media.value_or(0.0F) + *pu.media);
		}
		if (pu.copy) {
			agg.copy = std::make_optional(agg.copy.value_or(0.0F) + *pu.copy);
		}
	}

	const auto clamp100 = [](std::optional<float> v) -> std::optional<float> {
		return v ? std::make_optional(std::min(*v, 100.0F)) : std::nullopt;
	};
	agg.compute = clamp100(agg.compute);
	agg.render = clamp100(agg.render);
	agg.media = clamp100(agg.media);
	agg.copy = clamp100(agg.copy);
	return agg;
}

} // namespace fdinfo
