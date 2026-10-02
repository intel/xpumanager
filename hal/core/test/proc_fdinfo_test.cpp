/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "proc_fdinfo.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

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

constexpr const char *BDF = "0000:03:00.0";

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
