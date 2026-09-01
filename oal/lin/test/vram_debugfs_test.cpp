/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for the VRAM manager debugfs path resolver
 * (resolveVramMgrDebugfsPath), which selects between the per-tile entry of
 * current kernels and the legacy device-root entry of older ones.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "lin.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <utility>

namespace fs = std::filesystem;

namespace {

// Create a unique, empty scratch directory standing in for a device's debugfs
// directory (/sys/kernel/debug/dri/<bdf>) for one test case.
fs::path makeDeviceDir(const std::string &tag)
{
	fs::path dir = fs::temp_directory_path() / ("xpum_vram_debugfs_" + tag + "_" + std::to_string(::getpid()));
	std::error_code ec;
	fs::remove_all(dir, ec);
	fs::create_directories(dir);
	return dir;
}

// Layout of kernels with per-tile debugfs: <device>/tile0/vram_mm. The entry is
// asserted into existence: a case that expects the per-tile path would otherwise
// also pass on an empty fixture, since that is the path returned by default.
void writeTileEntry(const fs::path &deviceDir)
{
	fs::create_directories(deviceDir / "tile0");
	std::ofstream(deviceDir / "tile0" / "vram_mm") << "visible_avail: 21347MiB\n";
	REQUIRE(fs::exists(deviceDir / "tile0" / "vram_mm"));
}

// Layout of older kernels: <device>/vram0_mm.
void writeLegacyEntry(const fs::path &deviceDir)
{
	std::ofstream(deviceDir / "vram0_mm") << "visible_avail: 21347MiB\n";
	REQUIRE(fs::exists(deviceDir / "vram0_mm"));
}

// Makes a directory owner-accessible again, and removes it, when it goes out of
// scope, so a case that drops permissions cannot leave the scratch area poisoned.
class PermissionsRestore
{
	fs::path dir;

public:
	explicit PermissionsRestore(fs::path d) : dir(std::move(d)) {}
	~PermissionsRestore()
	{
		std::error_code ec;
		fs::permissions(dir, fs::perms::owner_all, ec);
		fs::remove_all(dir, ec);
	}
	PermissionsRestore(const PermissionsRestore &) = delete;
	PermissionsRestore &operator=(const PermissionsRestore &) = delete;
};

} // namespace

TEST_CASE("the per-tile entry is preferred when both layouts are present")
{
	const fs::path dir = makeDeviceDir("both");
	writeTileEntry(dir);
	writeLegacyEntry(dir);

	CHECK(resolveVramMgrDebugfsPath(dir.string()) == (dir / "tile0" / "vram_mm").string());

	fs::remove_all(dir);
}

TEST_CASE("the legacy entry is used when the per-tile entry is absent")
{
	// Regression guard: dropping this fallback made every vGPU query on a kernel
	// without per-tile debugfs report a failure to open tile0/vram_mm.
	const fs::path dir = makeDeviceDir("legacy");
	writeLegacyEntry(dir);

	CHECK(resolveVramMgrDebugfsPath(dir.string()) == (dir / "vram0_mm").string());

	fs::remove_all(dir);
}

TEST_CASE("an existing but empty tile0 directory still falls back to the legacy entry")
{
	const fs::path dir = makeDeviceDir("emptytile");
	fs::create_directories(dir / "tile0");
	writeLegacyEntry(dir);

	CHECK(resolveVramMgrDebugfsPath(dir.string()) == (dir / "vram0_mm").string());

	fs::remove_all(dir);
}

TEST_CASE("the per-tile path is returned when neither entry exists")
{
	// Callers report this path, so the message names the layout they expect.
	const fs::path dir = makeDeviceDir("neither");

	CHECK(resolveVramMgrDebugfsPath(dir.string()) == (dir / "tile0" / "vram_mm").string());

	fs::remove_all(dir);
}

TEST_CASE("a device directory that does not exist resolves without throwing")
{
	const fs::path dir = makeDeviceDir("absent");
	fs::remove_all(dir);

	std::string resolved;
	CHECK_NOTHROW(resolved = resolveVramMgrDebugfsPath(dir.string()));
	CHECK(resolved == (dir / "tile0" / "vram_mm").string());
}

TEST_CASE("a device path that cannot be walked resolves without throwing")
{
	// An error that leaves the file type unknown - as opposed to ENOENT and
	// ENOTDIR, which both count as "not found" - is what makes the throwing
	// std::filesystem::exists() overload raise filesystem_error. A symlink loop
	// produces one (ELOOP) for any user, so this covers the resolver's use of the
	// std::error_code overload where the debugfs case below cannot: as root.
	const fs::path dir = makeDeviceDir("loop");
	const fs::path looping = dir / "selfloop";
	fs::create_symlink("selfloop", looping);

	std::string resolved;
	CHECK_NOTHROW(resolved = resolveVramMgrDebugfsPath(looping.string()));
	CHECK(resolved == (looping / "tile0" / "vram_mm").string());

	fs::remove_all(dir);
}

TEST_CASE("an unreadable device directory resolves without throwing")
{
	// The EACCES case an unprivileged run hits for real: /sys/kernel/debug is
	// traversable by root only.
	if (::geteuid() == 0) {
		MESSAGE("skipped: permissions do not apply to root");
		return;
	}

	const fs::path parent = makeDeviceDir("noperm");
	const fs::path dir = parent / "0000:03:00.0";
	fs::create_directories(dir / "tile0");
	std::ofstream(dir / "tile0" / "vram_mm") << "visible_avail: 21347MiB\n";
	// Restore the mode however the case ends: a mode-000 directory left in the
	// scratch area would make a later run reusing this pid fail in its fixture.
	const PermissionsRestore restore{parent};
	fs::permissions(parent, fs::perms::none);

	std::string resolved;
	CHECK_NOTHROW(resolved = resolveVramMgrDebugfsPath(dir.string()));
	CHECK(resolved == (dir / "tile0" / "vram_mm").string());
}
