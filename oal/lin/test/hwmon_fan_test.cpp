/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for the Linux hwmon sysfs traversal that backs
 * getHwmonInputPaths() (oal/lin/hwmon_fan.cpp). The public entry point walks the
 * real /sys tree, so these tests exercise the internal, dependency-injected
 * helpers (oal::hwmon_fan_detail::scanHwmonInputDir / scanHwmonInputRoot)
 * against on-disk fixtures built in a unique temporary directory.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "hwmon_fan_internal.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

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
		path_ = base / ("oal_hwmon_fan_test_" + std::to_string(++counter));
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

// Populate a hwmon dir with a fan tachometer node: fan<n>_input = rpm.
void writeFan(const std::filesystem::path &hwmonDir, int n, const std::string &rpm)
{
	writeFile(hwmonDir / ("fan" + std::to_string(n) + "_input"), rpm + "\n");
}
} // namespace

TEST_CASE("scanHwmonInputDir: fanN_input resolves to zero-based index (fan1_input -> 0)")
{
	TempDir tmp;
	auto hwmon = tmp.path() / "hwmon1";
	writeFan(hwmon, 1, "1200");
	writeFan(hwmon, 2, "0"); // a stopped fan still has a node; path is recorded
	writeFan(hwmon, 12, "3000");

	std::map<uint32_t, std::string> out;
	oal::hwmon_fan_detail::scanHwmonInputDir(hwmon, "fan", out);

	REQUIRE(out.count(0) == 1);
	REQUIRE(out.count(1) == 1);
	REQUIRE(out.count(11) == 1);
	CHECK(out[0] == (hwmon / "fan1_input").string());
	CHECK(out[1] == (hwmon / "fan2_input").string());
	CHECK(out[11] == (hwmon / "fan12_input").string());
}

TEST_CASE("scanHwmonInputDir: strict filename validation rejects near-miss names")
{
	TempDir tmp;
	auto hwmon = tmp.path() / "hwmon1";
	// A well-formed fan so the directory is not empty.
	writeFan(hwmon, 1, "1200");
	// Near-miss names that must all be rejected by the strict parse.
	writeFile(hwmon / "fan_input", "1200\n");     // no digits
	writeFile(hwmon / "fanX_input", "1200\n");    // non-digit body
	writeFile(hwmon / "fan1_label", "cpu\n");     // wrong suffix
	writeFile(hwmon / "temp1_input", "50000\n");  // wrong prefix
	writeFile(hwmon / "fan2_inputx", "1200\n");   // suffix not at end
	writeFile(hwmon / "fan0_input", "1200\n");    // invalid 1-based zero

	std::map<uint32_t, std::string> out;
	oal::hwmon_fan_detail::scanHwmonInputDir(hwmon, "fan", out);

	// Only the strict fan1_input is accepted.
	CHECK(out.size() == 1);
	REQUIRE(out.count(0) == 1);
	CHECK(out[0] == (hwmon / "fan1_input").string());
}

TEST_CASE("scanHwmonInputRoot: discovers the fan node even when it is NOT the first hwmon dir")
{
	TempDir tmp;
	auto base = tmp.path() / "hwmon"; // stands in for /sys/bus/pci/devices/<BDF>/hwmon

	// hwmon0 is present but carries only a temperature node (no fanN_input).
	writeFile(base / "hwmon0" / "temp1_input", "50000\n");
	writeFile(base / "hwmon0" / "name", "xe\n");

	// hwmon1 carries the fan tachometer (mirrors the tested B70 layout).
	writeFan(base / "hwmon1", 1, "1234");

	std::map<uint32_t, std::string> out;
	int visited = oal::hwmon_fan_detail::scanHwmonInputRoot(base, "fan", out);

	CHECK(visited >= 1); // at least the productive node was reached
	REQUIRE(out.count(0) == 1);
	// The resolved path must live under the productive hwmon1 node, never hwmon0.
	CHECK(out[0] == (base / "hwmon1" / "fan1_input").string());
}

TEST_CASE("scanHwmonInputRoot: accepts only the first hwmon node that yields a valid entry")
{
	TempDir tmp;
	auto base = tmp.path() / "hwmon";
	// Only ONE productive fan node in the tree; a bare unproductive node must not
	// leave the mapping empty or shadow the real one.
	writeFile(base / "hwmon0" / "name", "empty\n"); // no fanN_input at all
	writeFan(base / "hwmon1", 1, "900");

	std::map<uint32_t, std::string> out;
	oal::hwmon_fan_detail::scanHwmonInputRoot(base, "fan", out);

	REQUIRE(out.count(0) == 1);
	CHECK(out[0] == (base / "hwmon1" / "fan1_input").string());
	CHECK(out.size() == 1);
}

TEST_CASE("scanHwmonInputDir: malformed / missing pieces do not produce entries")
{
	TempDir tmp;
	auto hwmon = tmp.path() / "hwmon1";
	// Strict-name rejects with no valid fan node present -> empty result.
	writeFile(hwmon / "fan_input", "bogus\n"); // no digits
	writeFile(hwmon / "name", "xe\n");         // not a fan node

	std::map<uint32_t, std::string> out;
	oal::hwmon_fan_detail::scanHwmonInputDir(hwmon, "fan", out);
	CHECK(out.empty());

	// A non-existent hwmon dir simply yields nothing (no crash).
	std::map<uint32_t, std::string> out2;
	oal::hwmon_fan_detail::scanHwmonInputDir(tmp.path() / "no_such_dir", "fan", out2);
	CHECK(out2.empty());
	CHECK(oal::hwmon_fan_detail::scanHwmonInputRoot(tmp.path() / "no_such_base", "fan", out2) == 0);
}

TEST_CASE("scanHwmonInputDir: oversized fanNNN..._input overflows and is rejected")
{
	TempDir tmp;
	auto hwmon = tmp.path() / "hwmon1";
	// 1-based node number 4294967297 -> zero-based 4294967296 overflows uint32.
	writeFile(hwmon / "fan4294967297_input", "1200\n");
	// A value far beyond 64-bit range (from_chars result_out_of_range).
	writeFile(hwmon / "fan99999999999999999999_input", "1200\n");

	std::map<uint32_t, std::string> out;
	oal::hwmon_fan_detail::scanHwmonInputDir(hwmon, "fan", out);
	CHECK(out.empty());

	// Boundary: the largest representable zero-based index (UINT32_MAX) parses,
	// i.e. 1-based node number UINT32_MAX+1 == 4294967296.
	writeFile(hwmon / "fan4294967296_input", "1200\n");
	oal::hwmon_fan_detail::scanHwmonInputDir(hwmon, "fan", out);
	REQUIRE(out.count(0xFFFFFFFFu) == 1);
	CHECK(out[0xFFFFFFFFu] == (hwmon / "fan4294967296_input").string());
}

TEST_CASE("scanHwmonInputRoot: two distinct device roots do not cross-resolve")
{
	TempDir tmpA;
	TempDir tmpB;
	auto baseA = tmpA.path() / "hwmon";
	auto baseB = tmpB.path() / "hwmon";
	writeFan(baseA / "hwmon0", 1, "1000");
	writeFan(baseB / "hwmon0", 1, "7700");

	std::map<uint32_t, std::string> outA;
	std::map<uint32_t, std::string> outB;
	oal::hwmon_fan_detail::scanHwmonInputRoot(baseA, "fan", outA);
	oal::hwmon_fan_detail::scanHwmonInputRoot(baseB, "fan", outB);

	REQUIRE(outA.count(0) == 1);
	REQUIRE(outB.count(0) == 1);
	// Each root resolves only within its own tree.
	CHECK(outA[0].rfind(baseA.string(), 0) == 0);
	CHECK(outB[0].rfind(baseB.string(), 0) == 0);
	CHECK(outA[0] != outB[0]);
}
