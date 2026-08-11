/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "hwmon_fan_utils.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

using xpum::hwmon::FanRpmSource;

// -----------------------------------------------------------------------------
// Temp hwmon-dir fixture for the sysfs fan-RPM read path.
//
// readFanInputFromDir is the state-free seam the instance fan::readSysfsFanRpm
// delegates to, so the actual fix site (read <dir>/fanN_input via the portable
// filesystem path + parseFanInputRpm) is covered here against a real temp
// directory -- portable, no /sys, no device, no POSIX-only pid/dir syscalls.
//
// A process-wide std::atomic counter gives each fixture directory a unique name
// without pid/tid syscalls, so concurrent test binaries never collide.
// -----------------------------------------------------------------------------
namespace {
std::filesystem::path makeTempHwmonDir()
{
	static std::atomic<uint64_t> counter{0};
	const uint64_t id = counter.fetch_add(1);
	std::error_code ec;
	std::filesystem::path dir =
		std::filesystem::temp_directory_path(ec) / ("xpum_fan_test_" + std::to_string(id));
	std::filesystem::remove_all(dir, ec);
	std::filesystem::create_directories(dir, ec);
	return dir;
}

void writeFile(const std::filesystem::path &path, const std::string &content)
{
	std::ofstream ofs(path);
	ofs << content;
}
} // namespace

// Tests for the pure helpers behind the Battlemage/xe sysfs hwmon fan RPM
// fallback validated on the Arc Pro B70. These are stateless and take no
// device/driver handle, so they exercise the parse/identity/discovery/selection
// contract directly without hardware.

// -----------------------------------------------------------------------------
// xpum::hwmon::parseFanInputRpm -- hwmon fanN_input contents -> RPM
// -----------------------------------------------------------------------------

TEST_CASE("parseFanInputRpm: well-formed value parses to RPM")
{
	int32_t rpm = -1;
	CHECK(xpum::hwmon::parseFanInputRpm("1234", rpm) == ZE_RESULT_SUCCESS);
	CHECK(rpm == 1234);

	// hwmon nodes are newline-terminated.
	rpm = -1;
	CHECK(xpum::hwmon::parseFanInputRpm("1234\n", rpm) == ZE_RESULT_SUCCESS);
	CHECK(rpm == 1234);

	// Leading/trailing whitespace is tolerated.
	rpm = -1;
	CHECK(xpum::hwmon::parseFanInputRpm("  987  ", rpm) == ZE_RESULT_SUCCESS);
	CHECK(rpm == 987);
}

TEST_CASE("parseFanInputRpm: 0 RPM is a valid reading (stopped fan)")
{
	// A stopped fan reports 0 and must be distinguishable from a failed read; 0
	// parses to SUCCESS with rpm == 0, NOT an error.
	int32_t rpm = -1;
	CHECK(xpum::hwmon::parseFanInputRpm("0", rpm) == ZE_RESULT_SUCCESS);
	CHECK(rpm == 0);

	rpm = -1;
	CHECK(xpum::hwmon::parseFanInputRpm("0\n", rpm) == ZE_RESULT_SUCCESS);
	CHECK(rpm == 0);
}

TEST_CASE("parseFanInputRpm: malformed / missing content is unsupported, never 0")
{
	int32_t rpm = 4242;

	// Empty (missing/unreadable node).
	CHECK(xpum::hwmon::parseFanInputRpm("", rpm) == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	// Non-numeric.
	CHECK(xpum::hwmon::parseFanInputRpm("abc", rpm) == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	CHECK(xpum::hwmon::parseFanInputRpm("N/A", rpm) == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	// Extra trailing tokens => malformed.
	CHECK(xpum::hwmon::parseFanInputRpm("1234 5", rpm) == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	// Negative RPM is impossible.
	CHECK(xpum::hwmon::parseFanInputRpm("-5", rpm) == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	// Absurdly large value that would overflow int32 is rejected.
	CHECK(xpum::hwmon::parseFanInputRpm("9999999999", rpm) == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

	// A malformed read must never be silently coerced to 0.
	CHECK(rpm == 4242);
}

// -----------------------------------------------------------------------------
// xpum::hwmon::readFanRpmFromPath -- the portable sysfs read site
//
// The OS-specific traversal that discovers fanN_input paths now lives in the OAL
// (getHwmonInputPaths, oal/lin/hwmon_fan.cpp, covered by oal/lin/test); this hal
// helper takes an already-resolved path and returns the parsed RPM. It is the
// state-free seam fan::readSysfsFanRpm delegates to, exercised here against real
// temp files -- portable, no /sys, no device, no POSIX-only pid/dir syscalls.
// -----------------------------------------------------------------------------

TEST_CASE("readFanRpmFromPath: valid fan1_input path parses to RPM")
{
	std::filesystem::path dir = makeTempHwmonDir();
	writeFile(dir / "fan1_input", "3210\n");

	int32_t rpm = -1;
	CHECK(xpum::hwmon::readFanRpmFromPath((dir / "fan1_input").string(), &rpm) == ZE_RESULT_SUCCESS);
	CHECK(rpm == 3210);

	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

TEST_CASE("readFanRpmFromPath: 0 RPM (stopped fan) is a valid reading")
{
	// A stopped fan reports 0; the sysfs branch must surface it as a successful
	// 0, not an error.
	std::filesystem::path dir = makeTempHwmonDir();
	writeFile(dir / "fan1_input", "0\n");

	int32_t rpm = -1;
	CHECK(xpum::hwmon::readFanRpmFromPath((dir / "fan1_input").string(), &rpm) == ZE_RESULT_SUCCESS);
	CHECK(rpm == 0);

	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

TEST_CASE("readFanRpmFromPath: missing fanN_input node is unsupported")
{
	// Empty dir: no fan1_input written. A missing node must report unsupported
	// and never fabricate a 0 reading.
	std::filesystem::path dir = makeTempHwmonDir();

	int32_t rpm = 4242;
	CHECK(xpum::hwmon::readFanRpmFromPath((dir / "fan1_input").string(), &rpm) == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	CHECK(rpm == 4242); // unchanged on failure

	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

TEST_CASE("readFanRpmFromPath: malformed fanN_input is unsupported, never 0")
{
	std::filesystem::path dir = makeTempHwmonDir();
	writeFile(dir / "fan1_input", "not-a-number");

	int32_t rpm = 4242;
	CHECK(xpum::hwmon::readFanRpmFromPath((dir / "fan1_input").string(), &rpm) == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	CHECK(rpm == 4242);

	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

TEST_CASE("readFanRpmFromPath: empty path is unsupported, null out is a null-pointer error")
{
	int32_t rpm = 4242;
	// No resolved path -> unsupported (the feature is simply absent).
	CHECK(xpum::hwmon::readFanRpmFromPath("", &rpm) == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	CHECK(rpm == 4242);

	// A null out-parameter is a caller bug and is classified distinctly as
	// INVALID_NULL_POINTER, not conflated with "unsupported".
	std::filesystem::path dir = makeTempHwmonDir();
	writeFile(dir / "fan1_input", "1200");
	CHECK(xpum::hwmon::readFanRpmFromPath((dir / "fan1_input").string(), nullptr) ==
		  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

TEST_CASE("readFanRpmFromPath: reads exactly the given path (identity)")
{
	// The reading must be attributed to the resolved path handed in, so distinct
	// fanN_input paths yield their own values.
	std::filesystem::path dir = makeTempHwmonDir();
	writeFile(dir / "fan1_input", "1000");
	writeFile(dir / "fan2_input", "2000");

	int32_t rpm = -1;
	CHECK(xpum::hwmon::readFanRpmFromPath((dir / "fan1_input").string(), &rpm) == ZE_RESULT_SUCCESS);
	CHECK(rpm == 1000);

	rpm = -1;
	CHECK(xpum::hwmon::readFanRpmFromPath((dir / "fan2_input").string(), &rpm) == ZE_RESULT_SUCCESS);
	CHECK(rpm == 2000);

	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
}

// -----------------------------------------------------------------------------
// xpum::hwmon::decideFanRpmSource -- the Level Zero vs sysfs selection contract
//
// This pure decision isolates the branch that fan::getAllSpeedsRpm / getSpeedRpm
// route through, so the selection is verified without a Level Zero device.
// -----------------------------------------------------------------------------

TEST_CASE("decideFanRpmSource: a failed enumeration propagates, never sysfs")
{
	CHECK(xpum::hwmon::decideFanRpmSource(ZE_RESULT_ERROR_DEVICE_LOST, 0) ==
		  FanRpmSource::PropagateEnumerationError);
	// A non-zero count is irrelevant once enumeration itself failed.
	CHECK(xpum::hwmon::decideFanRpmSource(ZE_RESULT_ERROR_UNINITIALIZED, 4) ==
		  FanRpmSource::PropagateEnumerationError);
}

TEST_CASE("decideFanRpmSource: no-fan / unprivileged enum result selects sysfs at any count")
{
	// Level Zero legitimately has no fans for us (UNSUPPORTED_FEATURE) or denies
	// access to an unprivileged process (INSUFFICIENT_PERMISSIONS). In both cases
	// the world-readable hwmon fanN_input is still available, so route to sysfs
	// regardless of the reported handle count.
	for (uint32_t count : {0u, 1u, 4u, 64u}) {
		CHECK(xpum::hwmon::decideFanRpmSource(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, count) ==
			  FanRpmSource::Sysfs);
		CHECK(xpum::hwmon::decideFanRpmSource(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, count) ==
			  FanRpmSource::Sysfs);
	}
}

TEST_CASE("decideFanRpmSource: success with zero handles selects sysfs")
{
	// Battlemage/xe: enumeration succeeds but reports no Level Zero fan handle.
	CHECK(xpum::hwmon::decideFanRpmSource(ZE_RESULT_SUCCESS, 0) == FanRpmSource::Sysfs);
}

TEST_CASE("decideFanRpmSource: success with >= 1 handle selects Level Zero")
{
	CHECK(xpum::hwmon::decideFanRpmSource(ZE_RESULT_SUCCESS, 1) == FanRpmSource::LevelZero);
	CHECK(xpum::hwmon::decideFanRpmSource(ZE_RESULT_SUCCESS, 8) == FanRpmSource::LevelZero);
}

TEST_CASE("decideFanRpmSource: a non-zero fan count never routes to sysfs")
{
	// Invariant guarding the inconsistent "fanCount > 0 but null handle array"
	// case: decideFanRpmSource returns LevelZero (not Sysfs) for any non-zero
	// count, so getAllSpeedsRpm handles the null-array guard as an internal error
	// instead of silently falling through to sysfs. The runtime null-array guard
	// inside getAllSpeedsRpm needs a real device to reach and is NOT mocked here
	// (we do not fabricate a device handle); this pure check locks the routing
	// property it depends on.
	for (uint32_t count : {1u, 2u, 3u, 64u}) {
		CHECK(xpum::hwmon::decideFanRpmSource(ZE_RESULT_SUCCESS, count) != FanRpmSource::Sysfs);
		CHECK(xpum::hwmon::decideFanRpmSource(ZE_RESULT_SUCCESS, count) == FanRpmSource::LevelZero);
	}
}
