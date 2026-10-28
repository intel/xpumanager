/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "proc_fdinfo.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <unordered_set>
#include <unistd.h>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

// RAII wrapper for a mkdtemp-created stand-in for /proc.
struct TempDir
{
	fs::path path;
	TempDir()
	{
		char tmpl[] = "/tmp/xpusmi_fdinfotest_XXXXXX";
		const char *p = mkdtemp(tmpl);
		REQUIRE(p != nullptr);
		path = p;
	}
	TempDir(const TempDir &) = delete;
	TempDir &operator=(const TempDir &) = delete;
	~TempDir() { fs::remove_all(path); }
};

// This process's own DRM client, as /proc/self/fd and /proc/self/fdinfo show it.
void addSelfClient(const fs::path &procRoot, const std::string &fd, const std::string &pciAddr,
				   const std::string &engineLines)
{
	const fs::path base = procRoot / "self";
	fs::create_directories(base / "fd");
	fs::create_directories(base / "fdinfo");
	fs::create_symlink("/dev/dri/renderD128", base / "fd" / fd);
	std::ofstream f(base / "fdinfo" / fd);
	f << "pos:\t0\n"
	  << "drm-driver:\txe\n"
	  << "drm-pdev:\t" << pciAddr << "\n"
	  << engineLines;
}

// Another process's DRM client, as fdinfo::capture() scans for it.
void addPidClient(const fs::path &procRoot, const std::string &pid, const std::string &fd, const std::string &driver,
				  const std::string &pciAddr, const std::string &engineLines)
{
	const fs::path base = procRoot / pid;
	fs::create_directories(base / "fd");
	fs::create_directories(base / "fdinfo");
	fs::create_symlink("/dev/dri/renderD128", base / "fd" / fd);
	std::ofstream f(base / "fdinfo" / fd);
	f << "pos:\t0\n"
	  << "drm-driver:\t" << driver << "\n"
	  << "drm-pdev:\t" << pciAddr << "\n"
	  << engineLines;
}

// A non-DRM fd: capture must skip it without reading its fdinfo.
void addPidFile(const fs::path &procRoot, const std::string &pid, const std::string &fd)
{
	const fs::path base = procRoot / pid;
	fs::create_directories(base / "fd");
	fs::create_directories(base / "fdinfo");
	fs::create_symlink("/dev/null", base / "fd" / fd);
	std::ofstream f(base / "fdinfo" / fd);
	f << "pos:\t0\n";
}

constexpr const char *BDF = "0000:03:00.0";
constexpr const char *BDF2 = "0000:83:00.0";

// PIDs of the snapshots, in ascending order.
std::vector<uint32_t> pidsOf(const std::vector<fdinfo::ProcessSnapshot> &snaps)
{
	std::vector<uint32_t> pids;
	for (const auto &s : snaps) {
		pids.push_back(s.pid);
	}
	std::ranges::sort(pids);
	return pids;
}

const fdinfo::ProcessSnapshot &snapOf(const std::vector<fdinfo::ProcessSnapshot> &snaps, uint32_t pid)
{
	const auto it = std::ranges::find(snaps, pid, &fdinfo::ProcessSnapshot::pid);
	REQUIRE(it != snaps.end());
	return *it;
}

} // namespace

TEST_CASE("capture: xe busy, total and capacity keys land in their own fields")
{
	const TempDir proc;
	addPidClient(proc.path, "4242", "5", "xe", BDF,
				 "drm-cycles-ccs:\t700\ndrm-total-cycles-ccs:\t9000\ndrm-engine-capacity-ccs:\t4\n"
				 "drm-total-vram0:\t65536 KiB\n");
	const auto snaps = fdinfo::capture(BDF, proc.path.string());
	REQUIRE(snaps.size() == 1);
	CHECK(snaps[0].pid == 4242);
	REQUIRE(snaps[0].engines.contains("ccs"));
	const auto &ccs = snaps[0].engines.at("ccs");
	CHECK(ccs.cycles == 700);		// not the capacity value
	CHECK(ccs.totalCycles == 9000); // not the busy value
	CHECK(snaps[0].engines.size() == 1);
}

TEST_CASE("capture: i915 drm-engine-<eng> is busy time with no total-cycles denominator")
{
	const TempDir proc;
	addPidClient(proc.path, "4343", "5", "i915", BDF,
				 "drm-engine-render:\t123456 ns\ndrm-engine-capacity-render:\t1\n");
	const auto snaps = fdinfo::capture(BDF, proc.path.string());
	REQUIRE(snaps.size() == 1);
	REQUIRE(snaps[0].engines.contains("rcs"));
	const auto &rcs = snaps[0].engines.at("rcs");
	CHECK(rcs.cycles == 123456);
	CHECK(rcs.totalCycles == 0); // the wall-clock sentinel delta() relies on
}

TEST_CASE("captureAll: one walk groups clients by device")
{
	const TempDir proc;
	addPidClient(proc.path, "100", "5", "xe", BDF, "drm-cycles-ccs:\t10\ndrm-total-cycles-ccs:\t100\n");
	addPidClient(proc.path, "200", "5", "xe", BDF2, "drm-cycles-bcs:\t20\ndrm-total-cycles-bcs:\t200\n");
	addPidClient(proc.path, "300", "5", "xe", BDF, "drm-cycles-ccs:\t30\ndrm-total-cycles-ccs:\t300\n");
	addPidClient(proc.path, "300", "6", "xe", BDF2, "drm-cycles-rcs:\t40\ndrm-total-cycles-rcs:\t400\n");

	const auto all = fdinfo::captureAll(proc.path.string());
	CHECK(all.size() == 2);
	CHECK(pidsOf(fdinfo::snapshotsFor(all, BDF)) == std::vector<uint32_t>{100, 300});
	CHECK(pidsOf(fdinfo::snapshotsFor(all, BDF2)) == std::vector<uint32_t>{200, 300});

	// Each device sees only the engines of its own fd of the shared process.
	const auto &onFirst = snapOf(fdinfo::snapshotsFor(all, BDF), 300);
	CHECK(onFirst.engines.size() == 1);
	CHECK(onFirst.engines.at("ccs").cycles == 30);
	const auto &onSecond = snapOf(fdinfo::snapshotsFor(all, BDF2), 300);
	CHECK(onSecond.engines.size() == 1);
	CHECK(onSecond.engines.at("rcs").cycles == 40);
}

TEST_CASE("captureAll: fds of one process on one device are merged")
{
	const TempDir proc;
	addPidClient(proc.path, "100", "5", "xe", BDF, "drm-cycles-ccs:\t100\ndrm-total-cycles-ccs:\t900\n");
	addPidClient(proc.path, "100", "6", "xe", BDF, "drm-cycles-ccs:\t50\ndrm-total-cycles-ccs:\t1000\n");

	const auto all = fdinfo::captureAll(proc.path.string());
	const auto &snaps = fdinfo::snapshotsFor(all, BDF);
	REQUIRE(snaps.size() == 1);
	const auto &ccs = snaps[0].engines.at("ccs");
	CHECK(ccs.cycles == 150);
	CHECK(ccs.totalCycles == 1000);
}

TEST_CASE("captureAll: non-DRM fds and clients without engine keys are skipped")
{
	const TempDir proc;
	addPidFile(proc.path, "100", "3");
	addPidClient(proc.path, "200", "5", "xe", BDF, "drm-total-vram0:\t65536 KiB\n");
	CHECK(fdinfo::captureAll(proc.path.string()).empty());
	CHECK(fdinfo::captureAll("/nonexistent/proc").empty());
}

TEST_CASE("capture: same snapshots as captureAll for that device")
{
	const TempDir proc;
	addPidClient(proc.path, "100", "5", "xe", BDF, "drm-cycles-ccs:\t10\ndrm-total-cycles-ccs:\t100\n");
	addPidClient(proc.path, "200", "5", "xe", BDF2, "drm-cycles-bcs:\t20\ndrm-total-cycles-bcs:\t200\n");
	addPidClient(proc.path, "300", "5", "xe", BDF, "drm-cycles-vcs:\t30\ndrm-total-cycles-vcs:\t300\n");

	const auto one = fdinfo::capture(BDF, proc.path.string());
	const auto all = fdinfo::captureAll(proc.path.string());
	const auto &fromAll = fdinfo::snapshotsFor(all, BDF);
	REQUIRE(pidsOf(one) == pidsOf(fromAll));
	for (const auto &snap : one) {
		const auto &other = snapOf(fromAll, snap.pid);
		REQUIRE(snap.engines.size() == other.engines.size());
		for (const auto &[eng, cnt] : snap.engines) {
			CHECK(other.engines.at(eng).cycles == cnt.cycles);
			CHECK(other.engines.at(eng).totalCycles == cnt.totalCycles);
		}
	}
}

TEST_CASE("snapshotsFor: a device without clients gives an empty vector")
{
	const TempDir proc;
	addPidClient(proc.path, "100", "5", "xe", BDF, "drm-cycles-ccs:\t10\ndrm-total-cycles-ccs:\t100\n");
	const auto all = fdinfo::captureAll(proc.path.string());
	CHECK(fdinfo::snapshotsFor(all, BDF2).empty());
	CHECK(fdinfo::snapshotsFor({}, BDF).empty());
}

TEST_CASE("engineCountsPerClass: capacity gives the count, a class without it has one engine")
{
	const TempDir proc;
	addSelfClient(proc.path, "3", BDF,
				  "drm-cycles-vcs:\t0\ndrm-total-cycles-vcs:\t100\ndrm-engine-capacity-vcs:\t4\n"
				  "drm-cycles-vecs:\t0\ndrm-total-cycles-vecs:\t100\ndrm-engine-capacity-vecs:\t2\n"
				  "drm-cycles-ccs:\t5\ndrm-total-cycles-ccs:\t100\n");
	const auto counts = fdinfo::engineCountsPerClass(BDF, proc.path.string());
	CHECK(counts.size() == 3);
	CHECK(counts.at("vcs") == 4);
	CHECK(counts.at("vecs") == 2);
	CHECK(counts.at("ccs") == 1);
}

TEST_CASE("engineCountsPerClass: several fds for the device report the largest count")
{
	const TempDir proc;
	addSelfClient(proc.path, "3", BDF, "drm-cycles-bcs:\t0\ndrm-total-cycles-bcs:\t1\n");
	addSelfClient(proc.path, "4", BDF, "drm-cycles-bcs:\t0\ndrm-total-cycles-bcs:\t1\ndrm-engine-capacity-bcs:\t3\n");
	CHECK(fdinfo::engineCountsPerClass(BDF, proc.path.string()).at("bcs") == 3);
}

TEST_CASE("engineCountsPerClass: nothing for another device, or without a /proc")
{
	const TempDir proc;
	addSelfClient(proc.path, "3", "0000:04:00.0", "drm-cycles-vcs:\t0\ndrm-engine-capacity-vcs:\t4\n");
	CHECK(fdinfo::engineCountsPerClass(BDF, proc.path.string()).empty());
	CHECK(fdinfo::engineCountsPerClass(BDF, "/nonexistent/proc").empty());
	CHECK(fdinfo::engineCountsPerClass("", proc.path.string()).empty());
}

namespace {

fdinfo::ProcessSnapshot snapOf(uint32_t pid, uint64_t wallNs,
							   std::initializer_list<std::pair<const char *, uint64_t>> busy)
{
	fdinfo::ProcessSnapshot s;
	s.pid = pid;
	s.wallNs = wallNs;
	for (const auto &[eng, cycles] : busy) {
		s.engines[eng].cycles = cycles;
		s.engines[eng].hasBusy = true;
	}
	return s;
}

} // namespace

TEST_CASE("ranWorkBetween: setup-only counters that do not move are not work")
{
	// Counters from mem_bench_int's fdinfo on an idle GPU: non-zero, but flat.
	const std::vector before{snapOf(1183, 100, {{"ccs", 7859}, {"bcs", 34913}, {"vcs", 0}})};
	const std::vector after{snapOf(1183, 200, {{"ccs", 7859}, {"bcs", 34913}, {"vcs", 0}})};
	const auto ran = fdinfo::ranWorkBetween(before, after);
	REQUIRE(ran.contains(1183));
	CHECK_FALSE(ran.at(1183));
}

TEST_CASE("ranWorkBetween: any rising engine counts, including one first seen at the end")
{
	const std::vector before{snapOf(1, 100, {{"ccs", 10}}), snapOf(2, 100, {{"ccs", 10}})};
	const std::vector after{snapOf(1, 200, {{"ccs", 10}, {"bcs", 5}}), snapOf(2, 200, {{"ccs", 11}})};
	const auto ran = fdinfo::ranWorkBetween(before, after);
	CHECK(ran.at(1));
	CHECK(ran.at(2));
}

TEST_CASE("ranWorkBetween: a falling counter, and a PID in only one snapshot, are left out")
{
	// PID 1's summed counter falls, as when it closes one of its DRM clients.
	const std::vector before{snapOf(1, 100, {{"ccs", 50}}), snapOf(2, 100, {{"ccs", 1}})};
	const std::vector after{snapOf(1, 200, {{"ccs", 3}}), snapOf(3, 200, {{"ccs", 9}})};
	const auto ran = fdinfo::ranWorkBetween(before, after);
	CHECK_FALSE(ran.contains(1));
	CHECK_FALSE(ran.contains(2));
	CHECK_FALSE(ran.contains(3));
}

TEST_CASE("ranWorkBetween: a vanished engine leaves activity unknown unless another engine rose")
{
	const std::vector before{snapOf(1, 100, {{"ccs", 10}, {"bcs", 5}}), snapOf(2, 100, {{"ccs", 10}, {"bcs", 5}})};
	const std::vector after{snapOf(1, 200, {{"ccs", 10}}), snapOf(2, 200, {{"ccs", 11}})};
	const auto ran = fdinfo::ranWorkBetween(before, after);
	CHECK_FALSE(ran.contains(1));
	CHECK(ran.at(2));
}

TEST_CASE("ranWorkBetween: a PID with no busy counter in both snapshots is left out")
{
	// Engine keys from capacity / total-cycles lines alone carry no activity data.
	fdinfo::ProcessSnapshot noBusy;
	noBusy.pid = 1;
	noBusy.wallNs = 100;
	noBusy.engines["ccs"].capacity = 4;
	noBusy.engines["ccs"].totalCycles = 1000;
	auto noBusyAfter = noBusy;
	noBusyAfter.wallNs = 200;
	noBusyAfter.engines["ccs"].totalCycles = 2000;

	const std::vector before{noBusy, snapOf(2, 100, {{"ccs", 10}})};
	const std::vector after{noBusyAfter, snapOf(2, 200, {{"bcs", 0}})};
	const auto ran = fdinfo::ranWorkBetween(before, after);
	CHECK_FALSE(ran.contains(1));
	CHECK_FALSE(ran.contains(2)); // busy counters on different engines: nothing to compare
}

TEST_CASE("ranWorkBetween: parsed fdinfo without busy counters is left out")
{
	const TempDir proc;
	addPidClient(proc.path, "1183", "3", "xe", BDF, "drm-total-cycles-ccs:\t1000\ndrm-engine-capacity-ccs:\t4\n");
	const auto before = fdinfo::capture(BDF, proc.path.string());
	const auto after = fdinfo::capture(BDF, proc.path.string());
	REQUIRE(before.size() == 1);
	CHECK(fdinfo::ranWorkBetween(before, after).empty());
}

TEST_CASE("ranWorkBetween: a rise within the busy floor is noise, above it work")
{
	// i915: busy ns over a 200 ms window.  1 ms is 0.5%, 4 ms is 2%.
	const std::vector before{snapOf(1, 0, {{"ccs", 100}}), snapOf(2, 0, {{"ccs", 100}})};
	const std::vector after{snapOf(1, 200'000'000, {{"ccs", 1'000'100}}), snapOf(2, 200'000'000, {{"ccs", 4'000'100}})};
	const auto ran = fdinfo::ranWorkBetween(before, after, 0.01);
	CHECK_FALSE(ran.at(1));
	CHECK(ran.at(2));
	CHECK(fdinfo::ranWorkBetween(before, after).at(1)); // no floor: any rise is work
}

TEST_CASE("ranWorkBetween: xe background busy time on a shared GPU is below the floor")
{
	// mem_bench_int's client on a 0x674f vLLM keeps busy: ~0.05% of the window.
	auto xeSnap = [](uint64_t wallNs, uint64_t bcs, uint64_t total) {
		auto s = snapOf(1, wallNs, {{"bcs", bcs}});
		s.engines["bcs"].totalCycles = total;
		return s;
	};
	const auto ran =
		fdinfo::ranWorkBetween({xeSnap(0, 32429, 437966541695)}, {xeSnap(245'000'000, 34824, 437971279008)}, 0.01);
	CHECK_FALSE(ran.at(1));
}

TEST_CASE("ranWorkBetween: a rise whose window length is unknown counts as work")
{
	// xe counters (totalCycles set) whose timestamp did not advance.
	auto before = snapOf(1, 0, {{"ccs", 10}});
	before.engines["ccs"].totalCycles = 5000;
	auto after = snapOf(1, 200'000'000, {{"ccs", 11}});
	after.engines["ccs"].totalCycles = 5000;
	CHECK(fdinfo::ranWorkBetween({before}, {after}, 0.01).at(1));
}

TEST_CASE("processAgeNs: start time from /proc/<pid>/stat against /proc/uptime")
{
	const TempDir proc;
	const auto ticks = static_cast<uint64_t>(sysconf(_SC_CLK_TCK));
	std::ofstream(proc.path / "uptime") << "1000.50 1.00\n";
	fs::create_directories(proc.path / "42");
	// A command name with spaces and a ')' must not shift the fields.
	std::ofstream stat(proc.path / "42" / "stat");
	stat << "42 (my (odd) proc) S 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 " << 990 * ticks << " 0 0\n";
	stat.close();
	const auto age = fdinfo::processAgeNs(42, proc.path.string());
	REQUIRE(age.has_value());
	CHECK(*age == 10'500'000'000ULL);
	CHECK_FALSE(fdinfo::processAgeNs(43, proc.path.string()).has_value());
}

TEST_CASE("lifetimeBusyNs: i915 busy counters are ns, summed over engines")
{
	const std::vector before{snapOf(1, 100, {{"ccs", 7859}, {"bcs", 34913}})};
	const std::vector after{snapOf(1, 200, {{"ccs", 7859}, {"bcs", 34913}})};
	const auto busy = fdinfo::lifetimeBusyNs(before, after);
	REQUIRE(busy.contains(1));
	CHECK(busy.at(1) == 7859 + 34913);
}

TEST_CASE("lifetimeBusyNs: xe ticks convert at the rate drm-total-cycles advances")
{
	// 100 ms window during which the 19.2 MHz GPU timestamp advanced 1.92M ticks.
	auto xeSnap = [](uint64_t wallNs, uint64_t totalCycles) {
		auto s = snapOf(1, wallNs, {{"ccs", 19'200}}); // 1 ms of busy time
		s.engines["ccs"].totalCycles = totalCycles;
		return s;
	};
	const std::vector before{xeSnap(0, 5'000'000)};
	const std::vector after{xeSnap(100'000'000, 6'920'000)};
	const auto busy = fdinfo::lifetimeBusyNs(before, after);
	REQUIRE(busy.contains(1));
	CHECK(busy.at(1) == 1'000'000);
}

TEST_CASE("lifetimeBusyNs: unknown when the xe timestamp does not advance, or the PID is in one snapshot")
{
	auto before = snapOf(1, 100, {{"ccs", 19'200}});
	before.engines["ccs"].totalCycles = 5'000'000;
	auto after = before;
	after.wallNs = 200;
	const auto busy = fdinfo::lifetimeBusyNs({before, snapOf(2, 100, {{"ccs", 1}})}, {after});
	CHECK(busy.empty());
}

TEST_CASE("drmDeviceCount: counts distinct drm-pdev values, not fds")
{
	const TempDir proc;
	addPidClient(proc.path, "1183", "3", "xe", "0000:41:00.0", "drm-cycles-ccs:\t7859\n");
	addPidClient(proc.path, "1183", "4", "xe", "0000:1a:00.0", "drm-cycles-ccs:\t646314451\n");
	addPidClient(proc.path, "1183", "5", "xe", "0000:45:00.0", "drm-cycles-ccs:\t132\n");
	addPidClient(proc.path, "1183", "6", "xe", "0000:1a:00.0", "drm-cycles-ccs:\t0\n"); // second fd, same GPU
	const std::unordered_set<std::string> gpus{"0000:41:00.0", "0000:1a:00.0", "0000:45:00.0"};
	CHECK(fdinfo::drmDeviceCount(1183, gpus, proc.path.string()) == 3);
}

TEST_CASE("drmDeviceCount: only the given GPUs count")
{
	// One listed dGPU, plus an iGPU and another vendor's GPU it is not given.
	const TempDir proc;
	addPidClient(proc.path, "1183", "3", "xe", "0000:03:00.0", "drm-cycles-ccs:\t0\n");
	addPidClient(proc.path, "1183", "4", "i915", "0000:00:02.0", "drm-engine-render:\t0 ns\n");
	addPidClient(proc.path, "1183", "5", "amdgpu", "0000:65:00.0", "");
	CHECK(fdinfo::drmDeviceCount(1183, {"0000:03:00.0"}, proc.path.string()) == 1);
	CHECK(fdinfo::drmDeviceCount(1183, {}, proc.path.string()) == 0);
}

TEST_CASE("drmDeviceCount: zero when the process cannot be read")
{
	const TempDir proc;
	CHECK(fdinfo::drmDeviceCount(1183, {"0000:03:00.0"}, proc.path.string()) == 0);
}

TEST_CASE("toProcUtil: video and video-enhance engines feed media and their own figures")
{
	const fdinfo::EngineUtilMap utils{{"vcs", 30.0F}, {"vecs", 70.0F}, {"ccs", 5.0F}};
	const auto pu = fdinfo::toProcUtil(utils);
	CHECK(pu.media == 70.0F);
	CHECK(pu.video == 30.0F);
	CHECK(pu.videoEnhance == 70.0F);
	CHECK(pu.compute == 5.0F);

	const auto onlyDecode = fdinfo::toProcUtil({{"video", 12.0F}});
	CHECK(onlyDecode.video == 12.0F);
	CHECK_FALSE(onlyDecode.videoEnhance.has_value());
}
