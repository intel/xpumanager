/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "../proc_fdinfo.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <unistd.h>
#include <utility>
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

// Normalise engine names from both i915 (slash-suffix) and xe (digit-suffix) formats
// to canonical short tokens.
//   i915 drm-engine-*: "render/0" → "rcs", "compute/0" → "ccs", "copy/0" → "bcs"
//   xe   drm-cycles-*: "rcs0"     → "rcs", "ccs1"      → "ccs", "bcs0"   → "bcs"
static std::string normaliseEngine(std::string_view raw)
{
	std::string name{raw};
	if (const auto p = name.find('/'); p != std::string::npos) {
		name.resize(p); // strip slash-based instance suffix ("render/0" -> "render")
	} else {
		while (!name.empty() && std::isdigit(static_cast<unsigned char>(name.back()))) {
			name.pop_back(); // strip digit-based instance suffix ("rcs0" -> "rcs")
		}
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

// One DRM client as one fdinfo file describes it.
struct DrmClient
{
	std::string pdev;
	std::unordered_map<std::string, EngineCounters> engines;
};

// Parse one /proc/<pid>/fdinfo/<fd> file; nullopt when it is not a DRM client.
static std::optional<DrmClient> parseDrmFdinfo(const std::string &path)
{
	std::ifstream fin{path};
	if (!fin) {
		return std::nullopt;
	}

	static constexpr std::string_view drivKey = "drm-driver";
	static constexpr std::string_view pdevKey = "drm-pdev";

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
		} else if (const auto ek = parseEngineKey(key)) {
			// i915 never emits Total, so its totalCycles stays 0: the wall-clock sentinel.
			EngineCounters &c = engines[ek->engine];
			switch (ek->field) {
			case EngineField::Busy:
				c.cycles = parseU64(val);
				c.hasBusy = true;
				break;
			case EngineField::Total:
				c.totalCycles = parseU64(val);
				break;
			case EngineField::Capacity:
				c.capacity = static_cast<uint32_t>(parseU64(val));
				break;
			}
		}
	}

	if (!isDrm) {
		return std::nullopt;
	}
	return DrmClient{std::move(pdev), std::move(engines)};
}

// Parse one /proc/<pid>/fdinfo/<fd> file.
// Returns an empty map if the file is not a DRM entry for expectedPci.
static std::unordered_map<std::string, EngineCounters>
parseFdinfo(const std::string &path, // NOLINT(bugprone-easily-swappable-parameters)
			const std::string &expectedPci)
{
	auto client = parseDrmFdinfo(path);
	if (!client || client->pdev != expectedPci) {
		return {};
	}
	return std::move(client->engines);
}

/* Public API */

/* Scan all open file descriptors of @p pid once and add a snapshot to @p out for each
 * device it is a DRM client of, merging the engine counters of that device's fds.
 * A device whose fds carry no engine counters gets no snapshot. */
static void captureOnePidAll(uint32_t pid, const std::string &procRoot, uint64_t wallNs, DeviceSnapshots &out)
{
	namespace fs = std::filesystem;

	const fs::path base = fs::path{procRoot} / std::to_string(pid);
	const fs::path fdDir = base / "fd";
	const fs::path fdinfoDir = base / "fdinfo";

	std::error_code ec;
	fs::directory_iterator fds{fdDir, ec};
	if (ec) {
		return;
	}

	std::unordered_map<std::string, ProcessSnapshot> perDevice;
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
		auto client = parseDrmFdinfo(fdinfoPath.string());
		if (!client || client->engines.empty()) {
			continue;
		}

		auto [it, inserted] = perDevice.try_emplace(client->pdev);
		ProcessSnapshot &snap = it->second;
		if (inserted) {
			snap.pid = pid;
			snap.wallNs = wallNs;
		}
		for (auto &[eng, cnt] : client->engines) {
			auto &dst = snap.engines[eng];
			dst.cycles += cnt.cycles;
			dst.totalCycles = std::max(dst.totalCycles, cnt.totalCycles);
			dst.hasBusy = dst.hasBusy || cnt.hasBusy;
		}
	}

	for (auto &[pdev, snap] : perDevice) {
		out[pdev].push_back(std::move(snap));
	}
}

DeviceSnapshots captureAll(const std::string &procRoot)
{
	namespace fs = std::filesystem;
	const uint64_t wallNs = monotonicNs();
	DeviceSnapshots result;

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

		captureOnePidAll(pid, procRoot, wallNs, result);
	}

	return result;
}

std::vector<ProcessSnapshot> capture(const std::string &pciAddr, // NOLINT(bugprone-easily-swappable-parameters)
									 const std::string &procRoot)
{
	if (pciAddr.empty()) {
		return {};
	}

	auto all = captureAll(procRoot);
	const auto it = all.find(pciAddr);
	return it == all.end() ? std::vector<ProcessSnapshot>{} : std::move(it->second);
}

const std::vector<ProcessSnapshot> &snapshotsFor(const DeviceSnapshots &all, std::string_view pciAddr)
{
	static const std::vector<ProcessSnapshot> none;
	const auto it = all.find(std::string{pciAddr});
	return it == all.end() ? none : it->second;
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

/**
 * @brief Maps a raw fdinfo engine key suffix to its zes_engine_type_flags_t bits.
 *
 * Accepts both driver spellings, which normaliseEngine() reduces to one token:
 * xe's drm-cycles-<eng> suffixes ("rcs0", "ccs1") and i915's drm-engine-<eng>
 * suffixes ("render/0", "compute/0").
 *
 * @param[in] rawEngine  Text between the drm-cycles- / drm-engine- prefix and ':'.
 *
 * @retval RENDER|3D  rcs, render
 * @retval COMPUTE    ccs, compute
 * @retval DMA        bcs, copy
 * @retval MEDIA      vcs, vecs, video, video-enhance
 * @retval 0          Any other name, including engine classes newer than this build.
 */
uint64_t engineFlagForKey(std::string_view rawEngine)
{
	const std::string name = normaliseEngine(rawEngine);
	if (name == "rcs") {
		return static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_RENDER) | static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_3D);
	}
	if (name == "ccs") {
		return static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_COMPUTE);
	}
	if (name == "bcs") {
		return static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_DMA);
	}
	if (name == "vcs" || name == "vecs") {
		return static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_MEDIA);
	}
	return 0;
}

std::optional<EngineKey> parseEngineKey(std::string_view key)
{
	// Longest prefix first: drm-engine-capacity- also starts with drm-engine-.
	static constexpr std::array<std::pair<std::string_view, EngineField>, 4> prefixes{{
		{"drm-engine-capacity-", EngineField::Capacity},
		{"drm-total-cycles-", EngineField::Total},
		{"drm-cycles-", EngineField::Busy}, // xe
		{"drm-engine-", EngineField::Busy}, // i915
	}};
	for (const auto &[prefix, field] : prefixes) {
		if (key.starts_with(prefix)) {
			if (key.size() == prefix.size()) {
				return std::nullopt;
			}
			return EngineKey{field, normaliseEngine(key.substr(prefix.size()))};
		}
	}
	return std::nullopt;
}

uint64_t enginesFromSnapshot(const ProcessSnapshot &snap)
{
	uint64_t flags = 0;
	for (const auto &[name, counters] : snap.engines) {
		// Zero cycles means the client never ran work on this engine; the key is
		// present regardless because xe lists every class the hardware exposes.
		if (counters.cycles == 0) {
			continue;
		}
		flags |= engineFlagForKey(name);
	}
	return flags;
}

/**
 * @brief Whether each process ran work on the device between two snapshots of it.
 *
 * Tells a client that is working now from one whose counters only show the work
 * it did once, such as the runtime's setup submissions on every GPU it opens.
 *
 * An idle client's counter is not always flat: on a GPU another process keeps
 * busy, xe charges an idle runtime context a little busy time (about 0.05% of
 * the window, measured on 0x674f next to vLLM).  A rise counts as work only when
 * the engine was busy for more than @p minBusyFraction of the window: busy ticks
 * over drm-total-cycles ticks on xe, busy ns over wall-clock ns on i915.
 *
 * @param[in] before           Snapshot taken at the start of the window.
 * @param[in] after            Snapshot of the same device taken at its end.
 * @param[in] minBusyFraction  Busy share of the window at or below which a rise is
 *                             noise, e.g. 0.01 for 1%.  0 counts any rise as work.
 * @return pid -> true when any engine's busy counter rose by more than
 *         @p minBusyFraction of the window, or an engine first seen in the after
 *         snapshot has a non-zero counter.  A PID missing from
 *         either snapshot, with no engine whose busy counter both snapshots
 *         report, or (when nothing rose) with a busy counter that fell or
 *         vanished, as when the process closes one of its DRM clients, is
 *         left out: its activity is unknown.
 */
std::unordered_map<uint32_t, bool> ranWorkBetween(const std::vector<ProcessSnapshot> &before,
												  const std::vector<ProcessSnapshot> &after, double minBusyFraction)
{
	std::unordered_map<uint32_t, const ProcessSnapshot *> bmap;
	bmap.reserve(before.size());
	for (const auto &s : before) {
		bmap[s.pid] = &s;
	}

	std::unordered_map<uint32_t, bool> result;
	for (const auto &snap : after) {
		const auto bit = bmap.find(snap.pid);
		if (bit == bmap.end()) {
			continue;
		}
		const auto &bengines = bit->second->engines;
		const uint64_t wallDelta = snap.wallNs > bit->second->wallNs ? snap.wallNs - bit->second->wallNs : 0;
		// The share of the window a rise of @p rise busy units covers, or nullopt when
		// the window length in those units is unknown.
		const auto busyFraction = [&](uint64_t rise, const EngineCounters &acnt,
									  const EngineCounters &bcnt) -> std::optional<double> {
			if (acnt.totalCycles > bcnt.totalCycles) {
				return static_cast<double>(rise) / static_cast<double>(acnt.totalCycles - bcnt.totalCycles); // xe
			}
			if (acnt.totalCycles == 0 && wallDelta > 0) {
				return static_cast<double>(rise) / static_cast<double>(wallDelta); // i915: ns
			}
			return std::nullopt;
		};
		// An engine first seen in the after snapshot counts as having run work
		// when its counter is non-zero. Engines without a busy counter carry no
		// activity data and are ignored.
		bool ran = false;
		bool comparable = false;
		for (const auto &[eng, acnt] : snap.engines) {
			if (!acnt.hasBusy) {
				continue;
			}
			const auto bit2 = bengines.find(eng);
			const bool hadBusy = bit2 != bengines.end() && bit2->second.hasBusy;
			comparable = comparable || hadBusy;
			if (!hadBusy) {
				ran = acnt.cycles > 0;
			} else if (acnt.cycles > bit2->second.cycles) {
				// A rise whose share of the window cannot be measured counts as work.
				const auto share = busyFraction(acnt.cycles - bit2->second.cycles, acnt, bit2->second);
				ran = !share || *share > minBusyFraction;
			}
			if (ran) {
				break;
			}
		}
		// Counters are summed over the PID's fds, so closing one can make a sum
		// fall or an engine vanish while another client still runs work: without
		// a rise, a falling or disappearing counter leaves activity unknown.
		const bool unknown = !ran && std::ranges::any_of(bengines, [&](const auto &kv) {
			if (!kv.second.hasBusy) {
				return false;
			}
			const auto ait = snap.engines.find(kv.first);
			return ait == snap.engines.end() || !ait->second.hasBusy || ait->second.cycles < kv.second.cycles;
		});
		if (ran || (comparable && !unknown)) {
			result[snap.pid] = ran;
		}
	}
	return result;
}

/**
 * @brief Total busy time each process has run on the device since it opened it.
 *
 * Sums the busy counters of the after snapshot over all engines and converts them
 * to nanoseconds.  i915 reports busy time in ns already.  xe reports GPU timestamp
 * ticks, converted at the tick rate seen across the window: drm-total-cycles
 * delta over wall-clock delta.  Tells the few ms of setup work an idle runtime
 * context did from a workload that merely did not submit during a short window.
 *
 * @param[in] before  Snapshot taken at the start of the window.
 * @param[in] after   Snapshot of the same device taken at its end.
 * @return pid -> busy ns.  A PID missing from either snapshot, or with an xe engine
 *         whose tick rate cannot be measured (drm-total-cycles did not advance), is
 *         left out: its busy time is unknown.
 */
std::unordered_map<uint32_t, uint64_t> lifetimeBusyNs(const std::vector<ProcessSnapshot> &before,
													  const std::vector<ProcessSnapshot> &after)
{
	std::unordered_map<uint32_t, const ProcessSnapshot *> bmap;
	bmap.reserve(before.size());
	for (const auto &s : before) {
		bmap[s.pid] = &s;
	}

	std::unordered_map<uint32_t, uint64_t> result;
	for (const auto &snap : after) {
		const auto bit = bmap.find(snap.pid);
		if (bit == bmap.end() || snap.wallNs <= bit->second->wallNs) {
			continue;
		}
		const auto &bengines = bit->second->engines;
		const auto wallDelta = static_cast<double>(snap.wallNs - bit->second->wallNs);

		double busyNs = 0.0;
		bool known = true;
		for (const auto &[eng, acnt] : snap.engines) {
			if (!acnt.hasBusy) {
				continue;
			}
			if (acnt.totalCycles == 0) {
				busyNs += static_cast<double>(acnt.cycles); // i915: already ns
				continue;
			}
			// xe: ticks; the timestamp's advance over the window gives ticks per ns.
			const auto bit2 = bengines.find(eng);
			if (bit2 == bengines.end() || acnt.totalCycles <= bit2->second.totalCycles) {
				known = false;
				break;
			}
			const auto dTotal = static_cast<double>(acnt.totalCycles - bit2->second.totalCycles);
			busyNs += static_cast<double>(acnt.cycles) * wallDelta / dTotal;
		}
		if (known) {
			result[snap.pid] = static_cast<uint64_t>(busyNs);
		}
	}
	return result;
}

/**
 * @brief How long a process has been running.
 *
 * Reads the start time from /proc/<pid>/stat (field 22, in clock ticks since boot)
 * and the time since boot from /proc/uptime.
 *
 * @param[in] pid       Process to inspect.
 * @param[in] procRoot  Defaults to "/proc"; override in tests to point at a fixture directory.
 * @return Age in ns, or nullopt when either file cannot be read or parsed.
 */
std::optional<uint64_t> processAgeNs(uint32_t pid, const std::string &procRoot)
{
	namespace fs = std::filesystem;
	double uptimeSec = 0.0;
	if (std::ifstream up{fs::path{procRoot} / "uptime"}; !(up >> uptimeSec)) {
		return std::nullopt;
	}

	std::ifstream st{fs::path{procRoot} / std::to_string(pid) / "stat"};
	std::string stat;
	if (!std::getline(st, stat)) {
		return std::nullopt;
	}
	// The command name in field 2 may contain spaces and parentheses, so count
	// fields from the last ')': field 3 (state) follows it, starttime is field 22.
	const auto paren = stat.rfind(')');
	if (paren == std::string::npos) {
		return std::nullopt;
	}
	std::istringstream fields{stat.substr(paren + 1)};
	std::string field;
	static constexpr int startTimeIndex = 22 - 3;
	for (int i = 0; i <= startTimeIndex; ++i) {
		if (!(fields >> field)) {
			return std::nullopt;
		}
	}
	uint64_t startTicks = 0;
	if (std::from_chars(field.data(), field.data() + field.size(), startTicks).ec != std::errc{}) {
		return std::nullopt;
	}
	const long ticksPerSec = sysconf(_SC_CLK_TCK);
	if (ticksPerSec <= 0) {
		return std::nullopt;
	}
	const double ageSec = uptimeSec - static_cast<double>(startTicks) / static_cast<double>(ticksPerSec);
	return ageSec > 0.0 ? static_cast<uint64_t>(ageSec * static_cast<double>(NS_PER_SEC)) : 0;
}

/**
 * @brief Number of distinct GPUs out of @p gpuPciAddrs a process holds a DRM client on.
 *
 * Counts the distinct drm-pdev values across the process's /dev/dri fds that name one
 * of @p gpuPciAddrs.  Clients on any other device, such as an iGPU xpu-smi does not
 * list or another vendor's GPU, are not counted.  A card and a render node of the
 * same GPU report the same drm-pdev, so they count once.
 *
 * @param[in] pid          Process to inspect.
 * @param[in] gpuPciAddrs  "DDDD:BB:DD.F" addresses of the GPUs to count, as devPciAddr() gives them.
 * @param[in] procRoot     Defaults to "/proc"; override in tests to point at a fixture directory.
 * @return Number of distinct devices; 0 when the process's fds cannot be read.
 */
uint32_t drmDeviceCount(uint32_t pid, const std::unordered_set<std::string> &gpuPciAddrs, const std::string &procRoot)
{
	namespace fs = std::filesystem;
	static constexpr std::string_view pdevKey = "drm-pdev:";

	const fs::path base = fs::path{procRoot} / std::to_string(pid);
	std::unordered_set<std::string> pdevs;
	std::error_code ec;
	for (const auto &fdEnt : fs::directory_iterator{base / "fd", ec}) {
		const fs::path target = fs::read_symlink(fdEnt.path(), ec);
		if (ec) {
			ec.clear();
			continue;
		}
		if (target.parent_path() != "/dev/dri") {
			continue;
		}
		std::ifstream fin{base / "fdinfo" / fdEnt.path().filename()};
		for (std::string line; std::getline(fin, line);) {
			if (!line.starts_with(pdevKey)) {
				continue;
			}
			std::string_view val = std::string_view{line}.substr(pdevKey.size());
			const size_t start = val.find_first_not_of(" \t");
			if (start != std::string_view::npos) {
				val = val.substr(start);
				std::string pdev{val.substr(0, val.find_last_not_of(" \t\r") + 1)};
				if (gpuPciAddrs.contains(pdev)) {
					pdevs.insert(std::move(pdev));
				}
			}
			break;
		}
	}
	return static_cast<uint32_t>(pdevs.size());
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
		} else if (name == "vcs" || name == "video") {
			maybeMax(pu.media, pct);
			maybeMax(pu.video, pct);
		} else if (name == "vecs" || name == "video-enhance") {
			maybeMax(pu.media, pct);
			maybeMax(pu.videoEnhance, pct);
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
		if (pu.video) {
			agg.video = std::make_optional(agg.video.value_or(0.0F) + *pu.video);
		}
		if (pu.videoEnhance) {
			agg.videoEnhance = std::make_optional(agg.videoEnhance.value_or(0.0F) + *pu.videoEnhance);
		}
	}

	const auto clamp100 = [](std::optional<float> v) -> std::optional<float> {
		return v ? std::make_optional(std::min(*v, 100.0F)) : std::nullopt;
	};
	agg.compute = clamp100(agg.compute);
	agg.render = clamp100(agg.render);
	agg.media = clamp100(agg.media);
	agg.copy = clamp100(agg.copy);
	agg.video = clamp100(agg.video);
	agg.videoEnhance = clamp100(agg.videoEnhance);
	return agg;
}

std::unordered_map<std::string, uint32_t> engineCountsPerClass(const std::string &pciAddr, const std::string &procRoot)
{
	namespace fs = std::filesystem;
	std::unordered_map<std::string, uint32_t> counts;
	if (pciAddr.empty()) {
		return counts;
	}
	const fs::path self = fs::path{procRoot} / "self";
	std::error_code ec;
	for (const auto &fdEnt : fs::directory_iterator{self / "fd", ec}) {
		const fs::path target = fs::read_symlink(fdEnt.path(), ec);
		if (ec) {
			ec.clear();
			continue;
		}
		if (target.parent_path() != "/dev/dri") {
			continue;
		}
		for (const auto &[eng, cnt] : parseFdinfo((self / "fdinfo" / fdEnt.path().filename()).string(), pciAddr)) {
			auto &n = counts[eng];
			n = std::max(n, std::max<uint32_t>(cnt.capacity, 1));
		}
	}
	return counts;
}

} // namespace fdinfo
