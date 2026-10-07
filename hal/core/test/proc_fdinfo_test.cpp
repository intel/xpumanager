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

// One DRM client: an fd symlink into /dev/dri and the fdinfo xe writes for it.
void addDrmClient(const fs::path &procRoot, unsigned pid, const std::string &pciAddr)
{
	const fs::path base = procRoot / std::to_string(pid);
	fs::create_directories(base / "fd");
	fs::create_directories(base / "fdinfo");
	fs::create_symlink("/dev/dri/renderD128", base / "fd" / "3");
	std::ofstream f(base / "fdinfo" / "3");
	f << "pos:\t0\n"
	  << "drm-driver:\txe\n"
	  << "drm-pdev:\t" << pciAddr << "\n"
	  << "drm-cycles-ccs:\t10\n"
	  << "drm-total-cycles-ccs:\t1000\n";
}

// Unsets the opt-out for the scope of a test, so the result does not depend on the
// environment the tests run in.
struct NoOptOut
{
	NoOptOut() { unsetenv("XPU_SMI_SYSMAN_ENGINE_UTIL"); }
	NoOptOut(const NoOptOut &) = delete;
	NoOptOut &operator=(const NoOptOut &) = delete;
	~NoOptOut() { unsetenv("XPU_SMI_SYSMAN_ENGINE_UTIL"); }
};

constexpr const char *BDF = "0000:03:00.0";

} // namespace

TEST_CASE("preferForEngineUtil: fdinfo is preferred when it sees a client of the device")
{
	const NoOptOut env;
	const TempDir proc;
	addDrmClient(proc.path, 1234, BDF);
	CHECK(fdinfo::preferForEngineUtil(BDF, proc.path.string()));
}

TEST_CASE("preferForEngineUtil: not preferred when fdinfo sees no client of this device")
{
	const NoOptOut env;
	const TempDir proc;
	addDrmClient(proc.path, 1234, "0000:04:00.0");
	CHECK_FALSE(fdinfo::preferForEngineUtil(BDF, proc.path.string()));
}

TEST_CASE("preferForEngineUtil: XPU_SMI_SYSMAN_ENGINE_UTIL keeps the sysman engine counters")
{
	const NoOptOut env;
	const TempDir proc;
	addDrmClient(proc.path, 1234, BDF);
	setenv("XPU_SMI_SYSMAN_ENGINE_UTIL", "1", 1);
	CHECK_FALSE(fdinfo::preferForEngineUtil(BDF, proc.path.string()));
}

TEST_CASE("preferForEngineUtil: not preferred without a readable /proc")
{
	const NoOptOut env;
	CHECK_FALSE(fdinfo::preferForEngineUtil(BDF, "/nonexistent/proc"));
	CHECK_FALSE(fdinfo::preferForEngineUtil("", "/proc"));
}
