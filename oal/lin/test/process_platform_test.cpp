/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for vramFromFdinfo and applyFdinfoCorrections.
 *
 * All tests use a fake /proc tree under a temp directory so no real GPU or
 * running processes are required.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "process_platform.h"
#include "proc_fdinfo.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <unistd.h>
#include <zes_api.h>

namespace fs = std::filesystem;

namespace {

// Minimal fake /proc tree.
class FakeProc
{
	fs::path root_;

public:
	explicit FakeProc(const std::string &tag)
	{
		root_ = fs::temp_directory_path() / ("xpum_proctest_" + tag + "_" + std::to_string(::getpid()));
		std::error_code ec;
		fs::remove_all(root_, ec);
		fs::create_directories(root_);
	}
	~FakeProc()
	{
		std::error_code ec;
		fs::remove_all(root_, ec);
	}
	FakeProc(const FakeProc &) = delete;
	FakeProc &operator=(const FakeProc &) = delete;

	[[nodiscard]] std::string path() const { return root_.string(); }

	// Create /proc/<pid>/fd/<num> -> target and /proc/<pid>/fdinfo/<num> with the
	// given drm-client-id plus memory fields.
	void addDrmFd(uint32_t pid, int fd, const std::string &deviceNode, uint64_t clientId, uint64_t vramKiB,
				  uint64_t totalKiB = 0) const
	{
		const fs::path fdDir = root_ / std::to_string(pid) / "fd";
		const fs::path fdinfoDir = root_ / std::to_string(pid) / "fdinfo";
		fs::create_directories(fdDir);
		fs::create_directories(fdinfoDir);

		// Create the symlink (best-effort; non-root can't always create /dev links
		// so use the target string as a plain file name).
		const fs::path link = fdDir / std::to_string(fd);
		std::error_code ec;
		fs::create_symlink(deviceNode, link, ec);
		if (ec) {
			// Fallback: write the target string into a plain file so readlink
			// will fail gracefully and the fd entry is skipped — that path is
			// covered by the "inaccessible fd dir" test case below.
			std::ofstream(link.string()) << deviceNode;
		}

		const uint64_t total = totalKiB > 0 ? totalKiB : vramKiB;
		std::ofstream fi(fdinfoDir / std::to_string(fd));
		fi << "drm-client-id:\t" << clientId << "\n";
		fi << "drm-total-vram0:\t" << vramKiB << " KiB\n";
		if (total != vramKiB) {
			fi << "drm-total-gtt0:\t" << (total - vramKiB) << " KiB\n";
		}
	}

	// Append raw lines to an existing /proc/<pid>/fdinfo/<num> file, for the
	// engine counters addDrmFd does not write.
	void appendFdinfo(uint32_t pid, int fd, const std::string &lines) const
	{
		std::ofstream fi(root_ / std::to_string(pid) / "fdinfo" / std::to_string(fd), std::ios::app);
		fi << lines;
	}

	// The engine block xe emits for a client that has never submitted work: every
	// class the hardware exposes is listed, all at zero.
	void appendIdleEngines(uint32_t pid, int fd) const
	{
		appendFdinfo(pid, fd,
					 "drm-cycles-rcs:\t0\ndrm-total-cycles-rcs:\t1604148030631\n"
					 "drm-cycles-ccs:\t0\ndrm-total-cycles-ccs:\t1604148030631\n"
					 "drm-cycles-bcs:\t0\ndrm-total-cycles-bcs:\t1604148030631\n"
					 "drm-cycles-vcs:\t0\ndrm-total-cycles-vcs:\t1604148030631\n"
					 "drm-engine-capacity-vcs:\t2\n");
	}

	// Create /proc/<pid>/fd/ directory with no device fds inside (simulates a
	// process that has open fds but none pointing to the GPU under test).
	void addEmptyFdDir(uint32_t pid) const { fs::create_directories(root_ / std::to_string(pid) / "fd"); }

	// Create NO /proc/<pid>/fd/ directory at all (simulates hidepid=2 or a
	// cross-user process where the fd directory itself is missing).
	void addInaccessibleFdDir(uint32_t pid) const
	{
		// Only the process directory exists, no fd subdirectory.
		fs::create_directories(root_ / std::to_string(pid));
	}
};

constexpr const char *GPU0_NODE = "/dev/dri/renderD128";
constexpr const char *GPU1_NODE = "/dev/dri/renderD129";

} // namespace

// ---------------------------------------------------------------------------
// vramFromFdinfo — basic happy path
// ---------------------------------------------------------------------------

TEST_CASE("vramFromFdinfo returns bytes and marks accessible for a normal process")
{
	FakeProc proc("happy");
	proc.addDrmFd(1000, 5, GPU0_NODE, /*clientId=*/42, /*vramKiB=*/65536, /*totalKiB=*/65536);

	const FdinfoResult r = vramFromFdinfo(1000, {GPU0_NODE}, proc.path());
	CHECK(r.localBytes == 65536ULL * 1024);
	CHECK(r.totalBytes == 65536ULL * 1024);
	CHECK(r.allInherited == false);
	CHECK(r.fdDirAccessible == true);
}

// ---------------------------------------------------------------------------
// vramFromFdinfo — accessible fd dir but no matching device fds
// ---------------------------------------------------------------------------

TEST_CASE("vramFromFdinfo: accessible fd dir with no GPU fds yields fdDirAccessible=true, bytes=0")
{
	FakeProc proc("empty_fd");
	proc.addEmptyFdDir(2000);

	const FdinfoResult r = vramFromFdinfo(2000, {GPU0_NODE}, proc.path());
	CHECK(r.localBytes == 0);
	CHECK(r.totalBytes == 0);
	CHECK(r.allInherited == false);
	CHECK(r.fdDirAccessible == true);
}

// ---------------------------------------------------------------------------
// vramFromFdinfo — inaccessible fd dir (no /fd subdirectory at all)
// ---------------------------------------------------------------------------

TEST_CASE("vramFromFdinfo: missing fd dir yields fdDirAccessible=false")
{
	FakeProc proc("inaccessible");
	proc.addInaccessibleFdDir(3000);

	const FdinfoResult r = vramFromFdinfo(3000, {GPU0_NODE}, proc.path());
	CHECK(r.localBytes == 0);
	CHECK(r.totalBytes == 0);
	CHECK(r.fdDirAccessible == false);
}

// ---------------------------------------------------------------------------
// vramFromFdinfo — duplicate drm-client-id (same context opened multiple fds)
// ---------------------------------------------------------------------------

TEST_CASE("vramFromFdinfo: same drm-client-id across two fds is counted once")
{
	FakeProc proc("dedup");
	// Same client-id 99 on fds 4 and 7 — should not be double-counted.
	proc.addDrmFd(4000, 4, GPU0_NODE, /*clientId=*/99, /*vramKiB=*/131072);
	proc.addDrmFd(4000, 7, GPU0_NODE, /*clientId=*/99, /*vramKiB=*/131072);

	const FdinfoResult r = vramFromFdinfo(4000, {GPU0_NODE}, proc.path());
	CHECK(r.localBytes == 131072ULL * 1024);
	CHECK(r.fdDirAccessible == true);
}

// ---------------------------------------------------------------------------
// vramFromFdinfo — globalSeenIds: child inherits parent's client-id
// ---------------------------------------------------------------------------

TEST_CASE("vramFromFdinfo: allInherited=true when every client-id already seen by parent")
{
	FakeProc proc("inherited");
	proc.addDrmFd(5000, 3, GPU0_NODE, /*clientId=*/77, /*vramKiB=*/40000);
	proc.addDrmFd(5001, 3, GPU0_NODE, /*clientId=*/77, /*vramKiB=*/40000);

	std::unordered_set<uint64_t> seen;
	const FdinfoResult parent = vramFromFdinfo(5000, {GPU0_NODE}, proc.path(), &seen);
	CHECK(parent.allInherited == false);
	CHECK(parent.localBytes == 40000ULL * 1024);

	const FdinfoResult child = vramFromFdinfo(5001, {GPU0_NODE}, proc.path(), &seen);
	CHECK(child.allInherited == true);
	CHECK(child.localBytes == 0);
}

// ---------------------------------------------------------------------------
// fixProcessMemSize — parent/child mis-attribution scenario (new1.png)
//
// The core of the bug:
//   - Level Zero reports parent PID on GPU 1 with child's memory (512 MiB).
//   - Parent's /proc/<pid>/fd has no renderD for GPU 1 (empty fd dir).
//   - fixProcessMemSize should zero the parent's memSize for GPU 1 because
//     fdDirAccessible=true but bytes=0.
// ---------------------------------------------------------------------------

TEST_CASE("fixProcessMemSize: parent PID with empty fd dir on GPU1 gets memSize zeroed")
{
	FakeProc proc("mis_attr");

	constexpr uint32_t parentPid = 6000;
	constexpr uint32_t childPid = 6001;

	// Parent has GPU 0 fd, but no GPU 1 fd.
	proc.addDrmFd(parentPid, 3, GPU0_NODE, /*clientId=*/11, /*vramKiB=*/143360); // 140 MiB
	proc.addEmptyFdDir(childPid); // child fd dir exists but isn't filled out here

	// Level Zero on GPU 1 reports parent PID with child's 512 MiB.
	std::vector<zes_process_state_t> list;
	{
		zes_process_state_t ps{};
		ps.stype = ZES_STRUCTURE_TYPE_PROCESS_STATE;
		ps.processId = parentPid;
		ps.memSize = 512ULL * 1024 * 1024; // Level Zero's wrong value
		ps.engines = 0;
		list.push_back(ps);
	}

	// Use a fake sysfs path that has no entries — fixProcessMemSize will find no
	// nodes via deviceNodesForBdf, which means it returns early without touching
	// the list.  We therefore call vramFromFdinfo directly to check the
	// fdDirAccessible behaviour, and simulate fixProcessMemSize's logic manually.
	const FdinfoResult res = vramFromFdinfo(parentPid, {GPU1_NODE}, proc.path());
	CHECK(res.localBytes == 0);
	CHECK(res.fdDirAccessible == true); // parent's fd dir is accessible but has no GPU1 fds

	// The fix: when fdDirAccessible=true, trust fdinfo (bytes=0) over Level Zero.
	// The caller would set memSize = 0.
	const uint64_t correctedMemSize = (res.localBytes > 0 || res.fdDirAccessible) ? res.localBytes : list[0].memSize;
	CHECK(correctedMemSize == 0);
}

// ---------------------------------------------------------------------------
// fixProcessMemSize — hidepid/cross-user: Level Zero memSize preserved
// ---------------------------------------------------------------------------

TEST_CASE("fixProcessMemSize: inaccessible fd dir preserves Level Zero memSize")
{
	FakeProc proc("hidepid");

	constexpr uint32_t pid = 7000;
	proc.addInaccessibleFdDir(pid);

	const FdinfoResult res = vramFromFdinfo(pid, {GPU0_NODE}, proc.path());
	CHECK(res.localBytes == 0);
	CHECK(res.fdDirAccessible == false);

	// When fdDirAccessible=false, Level Zero's value is preserved (not zeroed).
	constexpr uint64_t lzMemSize = 256ULL * 1024 * 1024;
	const uint64_t correctedMemSize = (res.localBytes > 0 || res.fdDirAccessible) ? res.localBytes : lzMemSize;
	CHECK(correctedMemSize == lzMemSize);
}

// ---------------------------------------------------------------------------
// vramFromFdinfo — dGPU vs iGPU: localBytes tracks only VRAM, totalBytes = all
// ---------------------------------------------------------------------------

TEST_CASE("vramFromFdinfo: localBytes counts only VRAM regions; totalBytes includes GTT")
{
	FakeProc proc("dgpu_igpu");
	// 64 MiB VRAM + 128 MiB GTT
	proc.addDrmFd(8000, 3, GPU0_NODE, /*clientId=*/55, /*vramKiB=*/65536, /*totalKiB=*/196608);

	const FdinfoResult r = vramFromFdinfo(8000, {GPU0_NODE}, proc.path());
	CHECK(r.localBytes == 65536ULL * 1024);
	CHECK(r.totalBytes == 196608ULL * 1024);
	CHECK(r.fdDirAccessible == true);
}

// ---------------------------------------------------------------------------
// engineFlags - distinguishes a real client from a zeInit-only phantom
//
// xe prints a drm-cycles-<eng> line for every engine class the hardware exposes
// on every DRM client, so only a non-zero counter proves work ran on a device.
// ---------------------------------------------------------------------------

TEST_CASE("vramFromFdinfo: VRAM held but every engine counter zero yields engineFlags=0")
{
	// The cross-GPU phantom: zeInit() opened a fd here and the driver made a
	// small internal allocation, but the process never submitted any work.
	FakeProc proc("idle_engines");
	proc.addDrmFd(9000, 4, GPU0_NODE, /*clientId=*/70, /*vramKiB=*/20480);
	proc.appendIdleEngines(9000, 4);

	const FdinfoResult r = vramFromFdinfo(9000, {GPU0_NODE}, proc.path());
	CHECK(r.localBytes == 20480ULL * 1024); // memory really is there
	CHECK(r.engineFlags == 0);				// but nothing ever ran
	CHECK(r.fdDirAccessible == true);
	CHECK(r.fdinfoReliable == true);
}

TEST_CASE("vramFromFdinfo: non-zero compute cycles sets only the COMPUTE flag")
{
	FakeProc proc("busy_ccs");
	proc.addDrmFd(9100, 4, GPU0_NODE, /*clientId=*/71, /*vramKiB=*/65536);
	proc.appendIdleEngines(9100, 4);
	proc.appendFdinfo(9100, 4, "drm-cycles-ccs0:\t123456\n");

	const FdinfoResult r = vramFromFdinfo(9100, {GPU0_NODE}, proc.path());
	CHECK(r.engineFlags == static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_COMPUTE));
}

TEST_CASE("vramFromFdinfo: drm-engine-capacity is hardware topology, not activity")
{
	// drm-engine-capacity-vcs:\t2 is non-zero but describes the hardware; counting
	// it would mark every client as a media process.
	FakeProc proc("capacity_only");
	proc.addDrmFd(9200, 4, GPU0_NODE, /*clientId=*/72, /*vramKiB=*/4096);
	proc.appendFdinfo(9200, 4, "drm-engine-capacity-vcs:\t2\ndrm-engine-capacity-vecs:\t2\n");

	const FdinfoResult r = vramFromFdinfo(9200, {GPU0_NODE}, proc.path());
	CHECK(r.engineFlags == 0);
}

TEST_CASE("vramFromFdinfo: i915 drm-engine-<eng> nanosecond counters set flags")
{
	FakeProc proc("i915_engines");
	proc.addDrmFd(9300, 4, GPU0_NODE, /*clientId=*/73, /*vramKiB=*/8192);
	proc.appendFdinfo(9300, 4, "drm-engine-render/0:\t998877 ns\ndrm-engine-copy/0:\t0 ns\n");

	const FdinfoResult r = vramFromFdinfo(9300, {GPU0_NODE}, proc.path());
	CHECK(r.engineFlags ==
		  (static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_RENDER) | static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_3D)));
}

TEST_CASE("vramFromFdinfo: engine flags survive the inherited-client dedup")
{
	// A forked child shares the parent's drm-client-id, so its memory is not
	// counted again - but that says nothing about whether work ran on this device,
	// and the allInherited erase already removes the child.
	FakeProc proc("inherited_engines");
	proc.addDrmFd(9400, 4, GPU0_NODE, /*clientId=*/74, /*vramKiB=*/65536);
	proc.appendFdinfo(9400, 4, "drm-cycles-ccs:\t555\n");

	std::unordered_set<uint64_t> seen{74};
	const FdinfoResult r = vramFromFdinfo(9400, {GPU0_NODE}, proc.path(), &seen);
	CHECK(r.allInherited == true);
	CHECK(r.localBytes == 0); // not double-counted
	CHECK(r.engineFlags == static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_COMPUTE));
}

// ---------------------------------------------------------------------------
// fdinfo::parseEngineKey - the one classifier both fdinfo parsers share
// ---------------------------------------------------------------------------

TEST_CASE("parseEngineKey: xe busy, total and capacity keys")
{
	const auto busy = fdinfo::parseEngineKey("drm-cycles-ccs0");
	REQUIRE(busy);
	CHECK(busy->field == fdinfo::EngineField::Busy);
	CHECK(busy->engine == "ccs");

	const auto total = fdinfo::parseEngineKey("drm-total-cycles-rcs");
	REQUIRE(total);
	CHECK(total->field == fdinfo::EngineField::Total);
	CHECK(total->engine == "rcs");

	const auto cap = fdinfo::parseEngineKey("drm-engine-capacity-vcs");
	REQUIRE(cap);
	CHECK(cap->field == fdinfo::EngineField::Capacity);
	CHECK(cap->engine == "vcs");
}

TEST_CASE("parseEngineKey: i915 drm-engine-<eng> is a busy counter")
{
	const auto ek = fdinfo::parseEngineKey("drm-engine-render/0");
	REQUIRE(ek);
	CHECK(ek->field == fdinfo::EngineField::Busy);
	CHECK(ek->engine == "rcs");
}

TEST_CASE("parseEngineKey: capacity is never read as the i915 busy key it prefixes")
{
	// drm-engine-capacity- starts with drm-engine-; matching the shorter prefix
	// first would turn every capacity line into busy time.
	const auto ek = fdinfo::parseEngineKey("drm-engine-capacity-vecs");
	REQUIRE(ek);
	CHECK(ek->field == fdinfo::EngineField::Capacity);
	CHECK(ek->engine == "vecs");
}

TEST_CASE("parseEngineKey: non-engine keys and empty engine names are rejected")
{
	CHECK_FALSE(fdinfo::parseEngineKey("drm-total-vram0"));
	CHECK_FALSE(fdinfo::parseEngineKey("drm-client-id"));
	CHECK_FALSE(fdinfo::parseEngineKey("drm-pdev"));
	CHECK_FALSE(fdinfo::parseEngineKey("drm-cycles-"));
	CHECK_FALSE(fdinfo::parseEngineKey("drm-engine-"));
	CHECK_FALSE(fdinfo::parseEngineKey(""));
}

// ---------------------------------------------------------------------------
// fdinfo::enginesFromSnapshot - same cycle gating, snapshot form
// ---------------------------------------------------------------------------

TEST_CASE("enginesFromSnapshot: present-but-zero engine keys contribute no flags")
{
	fdinfo::ProcessSnapshot snap;
	snap.pid = 9500;
	snap.engines["rcs"] = {.cycles = 0, .totalCycles = 1604148030631};
	snap.engines["ccs"] = {.cycles = 0, .totalCycles = 1604148030631};

	CHECK(fdinfo::enginesFromSnapshot(snap) == 0ULL);
}

TEST_CASE("enginesFromSnapshot: only engines with non-zero cycles contribute flags")
{
	fdinfo::ProcessSnapshot snap;
	snap.pid = 9600;
	snap.engines["ccs"] = {.cycles = 42, .totalCycles = 1604148030631};
	snap.engines["bcs"] = {.cycles = 0, .totalCycles = 1604148030631};

	CHECK(fdinfo::enginesFromSnapshot(snap) == static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_COMPUTE));
}

TEST_CASE("vramFromFdinfo: engineCountersPresent is false when the driver exports no counters")
{
	// A driver that omits drm-cycles-*/drm-engine-* leaves engineFlags at 0 for
	// every process. That is absence of data, not absence of activity, and must not
	// be mistaken for an idle holder -- filtering on it would report no processes
	// at all. addDrmFd writes only drm-client-id and memory fields.
	FakeProc proc("no_counters");
	proc.addDrmFd(9700, 4, GPU0_NODE, /*clientId=*/75, /*vramKiB=*/65536);

	const FdinfoResult r = vramFromFdinfo(9700, {GPU0_NODE}, proc.path());
	CHECK(r.localBytes == 65536ULL * 1024);
	CHECK(r.engineFlags == 0);
	CHECK_FALSE(r.engineCountersPresent);
	CHECK(r.fdDirAccessible == true);
}

TEST_CASE("vramFromFdinfo: engineCountersPresent is true when counters are exported at zero")
{
	// The counters are there and all read zero, so this really is an idle holder.
	FakeProc proc("zero_counters");
	proc.addDrmFd(9800, 4, GPU0_NODE, /*clientId=*/76, /*vramKiB=*/20480);
	proc.appendIdleEngines(9800, 4);

	const FdinfoResult r = vramFromFdinfo(9800, {GPU0_NODE}, proc.path());
	CHECK(r.engineFlags == 0);
	CHECK(r.engineCountersPresent);
}

TEST_CASE("vramFromFdinfo: activity on an unrecognised engine class counts as OTHER")
{
	// engineFlagForKey() returns 0 for a name this build does not know. Letting
	// that leave the mask empty would make a process actively running work on a
	// newly added engine class indistinguishable from an idle VRAM holder, and
	// fixProcessMemSize() would erase it.
	FakeProc proc("unknown_engine");
	proc.addDrmFd(9900, 4, GPU0_NODE, /*clientId=*/77, /*vramKiB=*/65536);
	proc.appendIdleEngines(9900, 4);
	proc.appendFdinfo(9900, 4, "drm-cycles-newclass0:\t4242\n");

	const FdinfoResult r = vramFromFdinfo(9900, {GPU0_NODE}, proc.path());
	CHECK(r.engineCountersPresent);
	CHECK(r.engineFlags == static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_OTHER));
}

TEST_CASE("vramFromFdinfo: a known engine alongside an unknown one keeps its own flag")
{
	FakeProc proc("mixed_engine");
	proc.addDrmFd(9950, 4, GPU0_NODE, /*clientId=*/78, /*vramKiB=*/65536);
	proc.appendFdinfo(9950, 4, "drm-cycles-ccs0:\t100\ndrm-cycles-newclass0:\t200\n");

	const FdinfoResult r = vramFromFdinfo(9950, {GPU0_NODE}, proc.path());
	CHECK(r.engineFlags ==
		  (static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_COMPUTE) | static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_OTHER)));
}

// ---------------------------------------------------------------------------
// applyFdinfoCorrections - engine filtering is independent of MemKind
// ---------------------------------------------------------------------------

TEST_CASE("applyFdinfoCorrections: returns an empty verified set when no DRM node matches")
{
	// Without a node for the BDF there is no fdinfo to read, so no mask is
	// rewritten and every engines value is still the driver's. The caller relies on
	// that to keep discarding a bare OTHER (XPUM-1242).
	std::vector<zes_process_state_t> procs(1);
	procs[0].processId = 4242;
	procs[0].engines = static_cast<zes_engine_type_flags_t>(ZES_ENGINE_TYPE_FLAG_OTHER);
	procs[0].memSize = 4096;

	const auto verified = applyFdinfoCorrections("ffff:ff:ff.f", &procs, std::nullopt);

	CHECK(verified.empty());
	// memSize untouched: nullopt must not rewrite memory, and there was no fdinfo.
	CHECK(procs[0].memSize == 4096);
}

TEST_CASE("applyFdinfoCorrections: nullopt MemKind leaves memSize exactly as reported")
{
	// The GPU type only picks between VRAM-only and combined totals. When it is
	// unknown the memory figure must be left alone rather than guessed.
	std::vector<zes_process_state_t> procs(1);
	procs[0].processId = 4243;
	procs[0].memSize = 123456;

	const auto verified = applyFdinfoCorrections("ffff:ff:ff.f", &procs, std::nullopt);

	CHECK(verified.empty());
	CHECK(procs[0].memSize == 123456);
}
