/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#undef INFO // doctest's INFO clashes with debug.h

#include "process_platform.h"

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

// ── Fixture helpers ──────────────────────────────────────────────────────────

// Writes a single fdinfo file and a matching fd symlink under procRoot.
// The symlink target is deviceNode (need not exist on disk; read_symlink
// returns the stored string, which is what vramFromFdinfo compares).
// Writes a fake fdinfo file using the given memory field name (e.g. "vram0", "gtt", "local0").
static void addFdWithField(const fs::path &procRoot, uint32_t pid, const std::string &fdNum,
						   const std::string &deviceNode, uint64_t clientId, uint64_t memKiB,
						   const std::string &memField)
{
	fs::path fdDir = procRoot / std::to_string(pid) / "fd";
	fs::path fdinfoDir = procRoot / std::to_string(pid) / "fdinfo";
	fs::create_directories(fdDir);
	fs::create_directories(fdinfoDir);

	fs::create_symlink(deviceNode, fdDir / fdNum);

	std::ofstream f(fdinfoDir / fdNum);
	f << "pos:\t0\n"
	  << "flags:\t02100002\n"
	  << "drm-driver:\txe\n"
	  << "drm-client-id:\t" << clientId << "\n"
	  << "drm-total-" << memField << ":\t" << memKiB << " KiB\n";
}

static void addFd(const fs::path &procRoot, uint32_t pid, const std::string &fdNum, const std::string &deviceNode,
				  uint64_t clientId, uint64_t vramKiB)
{
	addFdWithField(procRoot, pid, fdNum, deviceNode, clientId, vramKiB, "vram0");
}

// Builds a minimal fake /sys/class/drm subtree under `sysRoot` for one GPU.
//
// Creates:
//   sysRoot/<cardName>                               → symlink → sysRoot/devices/<bdf>/drm/<cardName>/
//   sysRoot/devices/<bdf>/drm/<node>/               → directory, for each node in drmNodes
//   sysRoot/devices/<bdf>/drm/<cardName>/device     → symlink → sysRoot/devices/<bdf>/
//
// This mirrors real sysfs so that deviceNodesForBdf can:
//   1. resolve card<N> via canonical() and find <bdf> in the path
//   2. traverse card<N>/device/drm/ to enumerate the associated DRM nodes
static void makeFakeDrmDevice(const fs::path &sysRoot, const std::string &bdf, const std::string &cardName,
							  const std::vector<std::string> &drmNodes)
{
	const fs::path devDrm = sysRoot / "devices" / bdf / "drm";
	for (const auto &node : drmNodes) {
		fs::create_directories(devDrm / node);
	}
	fs::create_symlink(sysRoot / "devices" / bdf, devDrm / cardName / "device");
	fs::create_symlink(devDrm / cardName, sysRoot / cardName);
}

// RAII wrapper for a mkdtemp-created temp directory.
struct TempDir
{
	fs::path path;
	TempDir()
	{
		char tmpl[] = "/tmp/xpusmi_proctest_XXXXXX";
		const char *p = mkdtemp(tmpl);
		REQUIRE(p != nullptr);
		path = p;
	}
	~TempDir() { fs::remove_all(path); }
};

// ── Tests ────────────────────────────────────────────────────────────────────

// Reproduces the reported Firefox bug: the same drm-client-id appears across
// four file descriptors, each reporting 1 950 572 KiB.  Without deduplication
// the total would be 7 692 MiB; with deduplication it should be ~1 977 MiB.
TEST_CASE("vramFromFdinfo: deduplicates by drm-client-id (Firefox scenario)")
{
	TempDir tmp;
	const uint32_t pid = 3643;
	const std::string rnode = "/dev/dri/renderD128";

	// client 47: four fds, all reporting the same 1 950 572 KiB
	addFd(tmp.path, pid, "21", rnode, 47, 1950572);
	addFd(tmp.path, pid, "50", rnode, 47, 1950572);
	addFd(tmp.path, pid, "51", rnode, 47, 1950572);
	addFd(tmp.path, pid, "52", rnode, 47, 1950572);
	// client 297: one fd, 37 036 KiB
	addFd(tmp.path, pid, "184", rnode, 297, 37036);
	// client 308: one fd, 37 036 KiB
	addFd(tmp.path, pid, "318", rnode, 308, 37036);
	// clients with zero vram (should contribute 0)
	addFd(tmp.path, pid, "53", rnode, 51, 0);
	addFd(tmp.path, pid, "54", rnode, 51, 0);
	addFd(tmp.path, pid, "55", rnode, 51, 0);
	addFd(tmp.path, pid, "170", rnode, 294, 0);
	addFd(tmp.path, pid, "176", rnode, 296, 0);
	addFd(tmp.path, pid, "179", rnode, 305, 0);
	addFd(tmp.path, pid, "297", rnode, 307, 0);

	// Wrong (pre-fix): (1950572*4 + 37036 + 37036) * 1024 bytes
	const uint64_t wrongBytes = (1950572ULL * 4 + 37036 + 37036) * 1024;
	// Correct (post-fix): (1950572 + 37036 + 37036) * 1024 bytes
	const uint64_t expectedBytes = (1950572ULL + 37036 + 37036) * 1024;

	auto result = vramFromFdinfo(pid, {rnode}, tmp.path.string());

	CHECK(result.bytes == expectedBytes);
	CHECK(result.bytes != wrongBytes);
	CHECK_FALSE(result.allInherited);
}

// A process with entirely distinct client-ids must sum all of them.
TEST_CASE("vramFromFdinfo: sums unique client-ids")
{
	TempDir tmp;
	const uint32_t pid = 1000;
	const std::string rnode = "/dev/dri/renderD128";

	addFd(tmp.path, pid, "10", rnode, 1, 100);
	addFd(tmp.path, pid, "11", rnode, 2, 200);
	addFd(tmp.path, pid, "12", rnode, 3, 300);

	auto result = vramFromFdinfo(pid, {rnode}, tmp.path.string());
	CHECK(result.bytes == 600ULL * 1024);
	CHECK_FALSE(result.allInherited);
}

// File descriptors pointing to a *different* device node must be ignored.
TEST_CASE("vramFromFdinfo: ignores fds for other device nodes")
{
	TempDir tmp;
	const uint32_t pid = 2000;
	const std::string rnode = "/dev/dri/renderD128";
	const std::string other = "/dev/dri/renderD129";

	addFd(tmp.path, pid, "10", rnode, 1, 500);
	addFd(tmp.path, pid, "11", other, 2, 9999); // different GPU — must not count

	auto result = vramFromFdinfo(pid, {rnode}, tmp.path.string());
	CHECK(result.bytes == 500ULL * 1024);
	CHECK_FALSE(result.allInherited);
}

// Passing multiple device nodes (card + renderD) for the same GPU is valid;
// each client-id is still counted only once even if it appears under both.
TEST_CASE("vramFromFdinfo: handles multiple device nodes for same GPU")
{
	TempDir tmp;
	const uint32_t pid = 3000;
	const std::string card = "/dev/dri/card0";
	const std::string render = "/dev/dri/renderD128";

	// client 10 appears under both card0 and renderD128 — count once
	addFd(tmp.path, pid, "20", card, 10, 1000);
	addFd(tmp.path, pid, "21", render, 10, 1000);
	// client 11 appears only under renderD128
	addFd(tmp.path, pid, "22", render, 11, 500);

	auto result = vramFromFdinfo(pid, {card, render}, tmp.path.string());
	CHECK(result.bytes == 1500ULL * 1024);
	CHECK_FALSE(result.allInherited);
}

// Empty device list → 0 without touching the filesystem.
TEST_CASE("vramFromFdinfo: returns 0 for empty device node list")
{
	TempDir tmp;
	auto result = vramFromFdinfo(9999, {}, tmp.path.string());
	CHECK(result.bytes == 0);
	CHECK_FALSE(result.allInherited);
}

// When a parent forks and the child inherits the DRM fd, both processes report
// the same drm-client-id.  Passing a shared globalSeenIds set ensures the
// inherited context is counted exactly once across both PIDs.
TEST_CASE("vramFromFdinfo: cross-process fork deduplicates shared client-id")
{
	TempDir tmp;
	const std::string rnode = "/dev/dri/renderD128";
	const uint32_t parentPid = 5000;
	const uint32_t childPid = 5001;

	// Shared context inherited via fork — same client-id, same VRAM in both.
	addFd(tmp.path, parentPid, "7", rnode, 99, 2000000);
	addFd(tmp.path, childPid, "7", rnode, 99, 2000000);
	// Child also has its own independent context.
	addFd(tmp.path, childPid, "8", rnode, 100, 50000);

	std::unordered_set<uint64_t> globalSeen;
	auto parentRes = vramFromFdinfo(parentPid, {rnode}, tmp.path.string(), &globalSeen);
	auto childRes = vramFromFdinfo(childPid, {rnode}, tmp.path.string(), &globalSeen);

	// client 99 is attributed to the parent; child receives only its own client 100.
	CHECK(parentRes.bytes == 2000000ULL * 1024);
	CHECK(childRes.bytes == 50000ULL * 1024);
	CHECK(parentRes.bytes + childRes.bytes == (2000000ULL + 50000ULL) * 1024);
	CHECK_FALSE(parentRes.allInherited);
	CHECK_FALSE(childRes.allInherited); // child has its own client 100
}

// Process with no open fds to our device → 0.
TEST_CASE("vramFromFdinfo: returns 0 when process has no fds to device")
{
	TempDir tmp;
	const uint32_t pid = 4000;
	const std::string rnode = "/dev/dri/renderD128";
	const std::string other = "/dev/dri/renderD129";

	addFd(tmp.path, pid, "10", other, 1, 8000);

	auto result = vramFromFdinfo(pid, {rnode}, tmp.path.string());
	CHECK(result.bytes == 0);
	CHECK_FALSE(result.allInherited); // no fds to our device at all → not inherited
}

// A forked child that holds only inherited fds (all client-ids already claimed
// by the parent) must be flagged as allInherited so fixProcessMemSize can drop it.
TEST_CASE("vramFromFdinfo: allInherited true when every client-id was already seen")
{
	TempDir tmp;
	const std::string rnode = "/dev/dri/renderD128";
	const uint32_t parentPid = 6000;
	const uint32_t childPid = 6001;

	// Parent holds the only GPU context.
	addFd(tmp.path, parentPid, "3", rnode, 42, 1024000);
	// Child inherits that fd — same client-id, no independent context.
	addFd(tmp.path, childPid, "3", rnode, 42, 1024000);

	std::unordered_set<uint64_t> globalSeen;
	auto parentRes = vramFromFdinfo(parentPid, {rnode}, tmp.path.string(), &globalSeen);
	auto childRes = vramFromFdinfo(childPid, {rnode}, tmp.path.string(), &globalSeen);

	CHECK(parentRes.bytes == 1024000ULL * 1024);
	CHECK_FALSE(parentRes.allInherited);

	CHECK(childRes.bytes == 0);
	CHECK(childRes.allInherited); // every client-id was claimed by the parent
}

// A child that mixes inherited and new contexts is NOT fully inherited and must
// not be erased — only its inherited portion is excluded from the byte count.
TEST_CASE("vramFromFdinfo: allInherited false when child has own context alongside inherited")
{
	TempDir tmp;
	const std::string rnode = "/dev/dri/renderD128";
	const uint32_t parentPid = 7000;
	const uint32_t childPid = 7001;

	addFd(tmp.path, parentPid, "3", rnode, 10, 500000);
	// Child inherits client 10 from parent, but also has its own client 11.
	addFd(tmp.path, childPid, "3", rnode, 10, 500000);
	addFd(tmp.path, childPid, "4", rnode, 11, 200000);

	std::unordered_set<uint64_t> globalSeen;
	auto parentRes2 = vramFromFdinfo(parentPid, {rnode}, tmp.path.string(), &globalSeen);
	CHECK(parentRes2.bytes == 500000ULL * 1024);
	auto childRes = vramFromFdinfo(childPid, {rnode}, tmp.path.string(), &globalSeen);

	// Only client 11 is new; client 10 is already in globalSeen.
	CHECK(childRes.bytes == 200000ULL * 1024);
	CHECK_FALSE(childRes.allInherited);
}

// iGPU reports memory under drm-total-gtt rather than drm-total-vram0.
// Both fields must be parsed and summed correctly.
TEST_CASE("vramFromFdinfo: iGPU drm-total-gtt field is counted")
{
	TempDir tmp;
	const uint32_t pid = 8000;
	const std::string rnode = "/dev/dri/renderD128";

	addFdWithField(tmp.path, pid, "10", rnode, 1, 1101108, "gtt");

	auto result = vramFromFdinfo(pid, {rnode}, tmp.path.string());
	CHECK(result.bytes == 1101108ULL * 1024);
	CHECK_FALSE(result.allInherited);
}

// drm-total-cycles-* lines are raw GPU utilisation counters, not KiB.
// They must be excluded from the memory sum.
TEST_CASE("vramFromFdinfo: drm-total-cycles fields are not counted as memory")
{
	TempDir tmp;
	const uint32_t pid = 8100;
	const std::string rnode = "/dev/dri/renderD128";

	// Manually write an fdinfo file containing cycles lines alongside a real
	// memory field to verify only the memory field is summed.
	fs::create_directories(tmp.path / std::to_string(pid) / "fd");
	fs::create_directories(tmp.path / std::to_string(pid) / "fdinfo");
	fs::create_symlink(rnode, tmp.path / std::to_string(pid) / "fd" / "5");
	{
		std::ofstream f(tmp.path / std::to_string(pid) / "fdinfo" / "5");
		f << "drm-client-id:\t70\n"
		  << "drm-total-gtt:\t1048576 KiB\n"
		  << "drm-total-cycles-rcs:\t38300634122\n"
		  << "drm-total-cycles-vcs:\t12000000000\n";
	}

	auto result = vramFromFdinfo(pid, {rnode}, tmp.path.string());
	// Only drm-total-gtt (1 GiB) should be counted; cycles values must not be added.
	CHECK(result.bytes == 1048576ULL * 1024);
	CHECK_FALSE(result.allInherited);
}

// When child PID appears first in the caller's process list (ZES gives no order
// guarantee), fixProcessMemSize must still attribute memory to the parent.
// The sort-by-PID in fixProcessMemSize is what makes this correct.
// This test verifies vramFromFdinfo itself is order-agnostic when globalSeenIds
// is driven by the caller in the right (parent-first) order.
TEST_CASE("vramFromFdinfo: parent-first ordering attributes memory to parent")
{
	TempDir tmp;
	const std::string rnode = "/dev/dri/renderD128";
	const uint32_t parentPid = 9000;
	const uint32_t childPid = 9001; // higher PID → was forked after parent

	addFd(tmp.path, parentPid, "3", rnode, 55, 800000);
	addFd(tmp.path, childPid, "3", rnode, 55, 800000); // same inherited client-id

	// Simulate fixProcessMemSize processing parent first (lower PID).
	std::unordered_set<uint64_t> globalSeen;
	auto parentRes = vramFromFdinfo(parentPid, {rnode}, tmp.path.string(), &globalSeen);
	auto childRes = vramFromFdinfo(childPid, {rnode}, tmp.path.string(), &globalSeen);

	CHECK(parentRes.bytes == 800000ULL * 1024);
	CHECK_FALSE(parentRes.allInherited);
	// Child only has the inherited context — must be flagged and erased.
	CHECK(childRes.bytes == 0);
	CHECK(childRes.allInherited);
}

// ── deviceNodesForBdf tests ──────────────────────────────────────────────────

TEST_CASE("deviceNodesForBdf: returns card and renderD nodes for a dGPU")
{
	TempDir tmp;
	makeFakeDrmDevice(tmp.path, "0000:4d:00.0", "card1", {"card1", "renderD128"});

	auto nodes = deviceNodesForBdf("0000:4d:00.0", tmp.path);
	CHECK(nodes.size() == 2);
	CHECK(std::ranges::find(nodes, "/dev/dri/card1") != nodes.end());
	CHECK(std::ranges::find(nodes, "/dev/dri/renderD128") != nodes.end());
}

TEST_CASE("deviceNodesForBdf: returns empty vector for unknown BDF")
{
	TempDir tmp;
	makeFakeDrmDevice(tmp.path, "0000:4d:00.0", "card1", {"card1", "renderD128"});

	auto nodes = deviceNodesForBdf("0000:ff:00.0", tmp.path);
	CHECK(nodes.empty());
}

TEST_CASE("deviceNodesForBdf: selects correct card when multiple GPUs are present")
{
	TempDir tmp;
	makeFakeDrmDevice(tmp.path, "0000:00:02.0", "card0", {"card0", "renderD128"}); // iGPU
	makeFakeDrmDevice(tmp.path, "0000:4d:00.0", "card1", {"card1", "renderD129"}); // dGPU

	auto igpu = deviceNodesForBdf("0000:00:02.0", tmp.path);
	CHECK(igpu.size() == 2);
	CHECK(std::ranges::find(igpu, "/dev/dri/card0") != igpu.end());
	CHECK(std::ranges::find(igpu, "/dev/dri/renderD128") != igpu.end());
	CHECK(std::ranges::find(igpu, "/dev/dri/renderD129") == igpu.end()); // dGPU node must not leak in

	auto dgpu = deviceNodesForBdf("0000:4d:00.0", tmp.path);
	CHECK(dgpu.size() == 2);
	CHECK(std::ranges::find(dgpu, "/dev/dri/card1") != dgpu.end());
	CHECK(std::ranges::find(dgpu, "/dev/dri/renderD129") != dgpu.end());
}

// ── Multi-GPU vramFromFdinfo test ────────────────────────────────────────────

// A process using two GPUs appears in each device's process list independently.
// fixProcessMemSize creates a separate globalSeenIds per device, so GPU0 and
// GPU1 client-ids are tracked in isolation — no cross-device deduplication.
TEST_CASE("vramFromFdinfo: multi-GPU process attributed independently per device")
{
	TempDir tmp;
	const uint32_t pid = 11000;
	const std::string gpu0 = "/dev/dri/renderD128";
	const std::string gpu1 = "/dev/dri/renderD129";

	addFd(tmp.path, pid, "10", gpu0, 1, 512000); // 500 MiB on GPU0, clientId 1
	addFd(tmp.path, pid, "11", gpu1, 2, 307200); // 300 MiB on GPU1, clientId 2

	// Simulate two separate fixProcessMemSize calls, each with its own globalSeen.
	std::unordered_set<uint64_t> seenGpu0;
	std::unordered_set<uint64_t> seenGpu1;

	auto resGpu0 = vramFromFdinfo(pid, {gpu0}, tmp.path.string(), &seenGpu0);
	auto resGpu1 = vramFromFdinfo(pid, {gpu1}, tmp.path.string(), &seenGpu1);

	CHECK(resGpu0.bytes == 512000ULL * 1024);
	CHECK(resGpu1.bytes == 307200ULL * 1024);
	CHECK_FALSE(resGpu0.allInherited);
	CHECK_FALSE(resGpu1.allInherited);
	// Client ids must not bleed across device boundaries.
	CHECK(seenGpu0.count(1) == 1);
	CHECK(seenGpu0.count(2) == 0);
	CHECK(seenGpu1.count(2) == 1);
	CHECK(seenGpu1.count(1) == 0);
}
