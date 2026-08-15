/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
// doctest defines INFO(expr) for test context; undef it so debug.h (pulled in
// via temperature.h) can define INFO(fmt, ...) for log-level gating.
#undef INFO

#include "temperature.h"
#include "hwmon_temperature_utils.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <string>

// Tests for temperature::mr4CodeToCelsius(), which converts a JEDEC LPDDR5
// MR4 thermal refresh code (0..7) to the max-of-range Celsius value. Values
// outside [0, 7], non-finite values, and non-integer values must be returned
// unchanged so non-LPDDR5 paths and already-converted readings are not
// disturbed.

TEST_CASE("mr4CodeToCelsius: maps each MR4 code to JEDEC max-of-range")
{
	CHECK(temperature::mr4CodeToCelsius(0.0) == doctest::Approx(0.0));
	CHECK(temperature::mr4CodeToCelsius(1.0) == doctest::Approx(45.0));
	CHECK(temperature::mr4CodeToCelsius(2.0) == doctest::Approx(85.0));
	CHECK(temperature::mr4CodeToCelsius(3.0) == doctest::Approx(95.0));
	CHECK(temperature::mr4CodeToCelsius(4.0) == doctest::Approx(105.0));
	CHECK(temperature::mr4CodeToCelsius(5.0) == doctest::Approx(110.0));
	CHECK(temperature::mr4CodeToCelsius(6.0) == doctest::Approx(125.0));
	CHECK(temperature::mr4CodeToCelsius(7.0) == doctest::Approx(125.0));
}

TEST_CASE("mr4CodeToCelsius: values outside [0, 7] pass through unchanged")
{
	CHECK(temperature::mr4CodeToCelsius(-1.0) == doctest::Approx(-1.0));
	CHECK(temperature::mr4CodeToCelsius(8.0) == doctest::Approx(8.0));
	CHECK(temperature::mr4CodeToCelsius(42.5) == doctest::Approx(42.5));
	CHECK(temperature::mr4CodeToCelsius(125.0) == doctest::Approx(125.0));
}

TEST_CASE("mr4CodeToCelsius: non-integer values in [0, 7] pass through unchanged")
{
	// An already-converted Celsius reading that happens to fall inside the
	// MR4 numeric range (e.g. 3.5 C) must not be re-mapped.
	CHECK(temperature::mr4CodeToCelsius(0.5) == doctest::Approx(0.5));
	CHECK(temperature::mr4CodeToCelsius(3.5) == doctest::Approx(3.5));
	CHECK(temperature::mr4CodeToCelsius(6.999) == doctest::Approx(6.999));
}

TEST_CASE("mr4CodeToCelsius: integers represented with small FP error are still mapped")
{
	// Tolerance is 1e-9; values within that of an integer code count as that
	// code.
	CHECK(temperature::mr4CodeToCelsius(3.0 + 1e-12) == doctest::Approx(95.0));
	CHECK(temperature::mr4CodeToCelsius(3.0 - 1e-12) == doctest::Approx(95.0));
}

TEST_CASE("mr4CodeToCelsius: non-finite inputs pass through unchanged")
{
	const double nan = std::numeric_limits<double>::quiet_NaN();
	const double inf = std::numeric_limits<double>::infinity();

	// NaN compares unequal to itself, so verify with std::isnan / std::isinf
	// rather than equality (which would also trip -Wfloat-equal).
	CHECK(std::isnan(temperature::mr4CodeToCelsius(nan)));
	CHECK(std::isinf(temperature::mr4CodeToCelsius(inf)));
	CHECK(temperature::mr4CodeToCelsius(inf) > 0.0);
	CHECK(std::isinf(temperature::mr4CodeToCelsius(-inf)));
	CHECK(temperature::mr4CodeToCelsius(-inf) < 0.0);
}

// ---------------------------------------------------------------------------
// Fallback-source decision tests (the central contract). decideTempSource()
// routes a Level Zero getState() result: SUCCESS keeps the L0 value,
// UNSUPPORTED_FEATURE (no such sensor) or INSUFFICIENT_PERMISSIONS (Level Zero
// denied an unprivileged process) route to the world-readable sysfs node, and
// genuine device / state errors propagate so a device-loss is not masked by a
// sysfs read. perTileShouldFallback() is the per-tile analogue keyed on whether
// a matching sensor was enumerated.
// ---------------------------------------------------------------------------

TEST_CASE("decideTempSource: unsupported or permission-denied uses sysfs; hard errors propagate")
{
	using xpum::hwmon::decideTempSource;
	using xpum::hwmon::TempSource;

	// A successful Level Zero read is authoritative and NEVER falls back.
	CHECK(decideTempSource(ZE_RESULT_SUCCESS) == TempSource::UseLevelZero);

	// An unsupported sensor routes to sysfs; insufficient permissions does too.
	CHECK(decideTempSource(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) == TempSource::UseSysfs);

	// Level Zero denying an unprivileged process routes to the world-readable
	// sysfs node, matching the unsupported-sensor case: normal users can still
	// read the PCI hwmon node even when the L0 query needs privilege.
	CHECK(decideTempSource(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS) == TempSource::UseSysfs);

	// Genuine errors from an existing sensor must propagate, not be masked -- this
	// is the exact bug class the hardening pass protects at the decision point.
	CHECK(decideTempSource(ZE_RESULT_ERROR_DEVICE_LOST) == TempSource::PropagateError);
	CHECK(decideTempSource(ZE_RESULT_ERROR_UNKNOWN) == TempSource::PropagateError);
}

TEST_CASE("perTileShouldFallback: falls back only when no matching sensor was found")
{
	CHECK(xpum::hwmon::perTileShouldFallback(true) == false);
	CHECK(xpum::hwmon::perTileShouldFallback(false) == true);
}

// ---------------------------------------------------------------------------
// sysfs hwmon fallback tests (Arc Pro B70 / xe). These exercise the pure,
// internal xpum::hwmon helpers that back the fallback: strict filename parsing,
// the shared bounds-checked read path, and multi-hwmon discovery. Fixtures are
// built in a unique temporary directory and removed at the end of each case.
// ---------------------------------------------------------------------------

namespace
{
// RAII unique temp directory for building hwmon fixtures on disk. Uniqueness
// comes from a process-wide atomic counter (portable; no POSIX process-id call).
class TempDir
{
public:
	TempDir()
	{
		static std::atomic<uint64_t> counter{0};
		std::filesystem::path base = std::filesystem::temp_directory_path();
		path_ = base / ("xpum_temp_test_" + std::to_string(++counter));
		std::filesystem::remove_all(path_);
		std::filesystem::create_directories(path_);
	}
	~TempDir()
	{
		std::error_code ec;
		std::filesystem::remove_all(path_, ec);
	}
	const std::filesystem::path &path() const { return path_; }

private:
	std::filesystem::path path_;
};

// Write `content` to `p`, creating parent dirs as needed.
void writeFile(const std::filesystem::path &p, const std::string &content)
{
	std::filesystem::create_directories(p.parent_path());
	std::ofstream f(p);
	f << content;
}
} // namespace

TEST_CASE("readTempInputFile: parses valid milli-Celsius and enforces physical bounds")
{
	TempDir tmp;
	double c = -1234.0;

	// Valid: 67000 milli -> 67.0 C.
	auto good = tmp.path() / "good_input";
	writeFile(good, "67000\n");
	CHECK(xpum::hwmon::readTempInputFile(good.string(), &c) == ZE_RESULT_SUCCESS);
	CHECK(c == doctest::Approx(67.0));

	// The B70 package reading on the tested hardware (50000 -> 50.0 C).
	auto pkg = tmp.path() / "pkg_input";
	writeFile(pkg, "50000\n");
	CHECK(xpum::hwmon::readTempInputFile(pkg.string(), &c) == ZE_RESULT_SUCCESS);
	CHECK(c == doctest::Approx(50.0));

	// Malformed / non-numeric content -> error, out-param untouched.
	double before = 42.0;
	auto bad = tmp.path() / "bad_input";
	writeFile(bad, "not_a_number\n");
	CHECK(xpum::hwmon::readTempInputFile(bad.string(), &before) == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	CHECK(before == doctest::Approx(42.0));

	// Above the upper reasonable bound (200000 -> 200 C) -> reject.
	auto hot = tmp.path() / "hot_input";
	writeFile(hot, "200000\n");
	CHECK(xpum::hwmon::readTempInputFile(hot.string(), &c) == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

	// A legitimately sub-zero reading (-50000 milli -> -50.0 C) is now ACCEPTED.
	// The fallback is condition-based (not PCI-id gated), so the lower bound is a
	// physical-validity floor (absolute zero), not a device-specific 0 C floor
	// that would wrongly reject a real subzero reading on another matching system.
	auto sub = tmp.path() / "subzero_input";
	writeFile(sub, "-50000\n");
	CHECK(xpum::hwmon::readTempInputFile(sub.string(), &c) == ZE_RESULT_SUCCESS);
	CHECK(c == doctest::Approx(-50.0));

	// Below absolute zero (-273.15 C) is physically impossible -> reject.
	auto impossible = tmp.path() / "impossible_input";
	writeFile(impossible, "-300000\n"); // -300 C
	CHECK(xpum::hwmon::readTempInputFile(impossible.string(), &c) == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

	// Missing file -> reject (stream does not open).
	CHECK(xpum::hwmon::readTempInputFile((tmp.path() / "does_not_exist").string(), &c) ==
		  ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

	// Null out-pointer -> explicit null-pointer error.
	CHECK(xpum::hwmon::readTempInputFile(good.string(), nullptr) == ZE_RESULT_ERROR_INVALID_NULL_POINTER);
}
