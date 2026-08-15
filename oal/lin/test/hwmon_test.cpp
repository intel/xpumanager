/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for the Linux hwmon sysfs traversal that backs
 * getHwmonLabelPaths() (oal/lin/hwmon.cpp). The public entry point walks the
 * real /sys tree, so these tests exercise the internal, dependency-injected
 * helpers (oal::hwmon_detail::scanHwmonDir / scanHwmonRoot) against on-disk
 * fixtures built in a unique temporary directory.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "hwmon_internal.h"

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
		path_ = base / ("oal_hwmon_test_" + std::to_string(++counter));
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

// Populate a hwmon dir with a labelled temp sensor: temp<idx>_label = label and
// temp<idx>_input = milli.
void writeSensor(const std::filesystem::path &hwmonDir, int idx, const std::string &label, const std::string &milli)
{
	writeFile(hwmonDir / ("temp" + std::to_string(idx) + "_label"), label + "\n");
	writeFile(hwmonDir / ("temp" + std::to_string(idx) + "_input"), milli + "\n");
}
} // namespace

TEST_CASE("scanHwmonDir: records label -> tempN_input for well-formed sensors only")
{
	TempDir tmp;
	auto hwmon = tmp.path() / "hwmon1";
	writeSensor(hwmon, 2, "pkg", "50000");
	writeSensor(hwmon, 3, "vram", "52000");
	// A label with no sibling _input must be skipped (missing input).
	writeFile(hwmon / "temp4_label", "mctrl\n");
	// A non-temp file must be ignored.
	writeFile(hwmon / "name", "xe\n");

	std::map<std::string, std::string> out;
	oal::hwmon_detail::scanHwmonDir(hwmon, "temp", out);

	REQUIRE(out.count("pkg") == 1);
	REQUIRE(out.count("vram") == 1);
	CHECK(out.count("mctrl") == 0); // skipped: no temp4_input
	CHECK(out["pkg"] == (hwmon / "temp2_input").string());
	CHECK(out["vram"] == (hwmon / "temp3_input").string());
}

TEST_CASE("scanHwmonDir: strict filename validation rejects near-miss names")
{
	TempDir tmp;
	auto hwmon = tmp.path() / "hwmon1";
	// A well-formed sensor so the directory is not empty.
	writeSensor(hwmon, 2, "pkg", "50000");
	// Near-miss label names, each paired with a plausible input so ONLY the name
	// validation can be what rejects them.
	writeFile(hwmon / "temperature_label", "bad\n"); // non-digit body
	writeFile(hwmon / "temperature_input", "50000\n");
	writeFile(hwmon / "temp_label", "bad\n"); // no digits
	writeFile(hwmon / "temp_input", "50000\n");
	writeFile(hwmon / "temp5_labelx", "bad\n"); // suffix not at end
	writeFile(hwmon / "temp5_input", "50000\n");
	writeFile(hwmon / "temp6a_label", "bad\n"); // non-digit in body
	writeFile(hwmon / "temp6a_input", "50000\n");

	std::map<std::string, std::string> out;
	oal::hwmon_detail::scanHwmonDir(hwmon, "temp", out);

	// Only the strict temp2_label is accepted.
	CHECK(out.size() == 1);
	REQUIRE(out.count("pkg") == 1);
	CHECK(out["pkg"] == (hwmon / "temp2_input").string());
}

TEST_CASE("scanHwmonRoot: discovers the labelled node even when it is NOT the first hwmon dir")
{
	TempDir tmp;
	auto base = tmp.path() / "hwmon"; // stands in for /sys/bus/pci/devices/<BDF>/hwmon

	// hwmon0 is present but has no temperature labels at all.
	std::filesystem::create_directories(base / "hwmon0");
	writeFile(base / "hwmon0" / "name", "empty\n");

	// hwmon1 carries the pkg/vram sensors (mirrors the tested B70 layout).
	writeSensor(base / "hwmon1", 2, "pkg", "50000");
	writeSensor(base / "hwmon1", 3, "vram", "52000");

	std::map<std::string, std::string> out;
	int visited = oal::hwmon_detail::scanHwmonRoot(base, "temp", out);

	CHECK(visited == 2); // both hwmon0 and hwmon1 were scanned, not just the first
	REQUIRE(out.count("pkg") == 1);
	REQUIRE(out.count("vram") == 1);
	CHECK(out["pkg"] == (base / "hwmon1" / "temp2_input").string());
	CHECK(out["vram"] == (base / "hwmon1" / "temp3_input").string());
}

TEST_CASE("scanHwmonDir: first match wins for a duplicated label")
{
	TempDir tmp;
	auto base = tmp.path() / "hwmon";
	// hwmon0 has pkg, hwmon1 also has pkg with a different value. Accumulating
	// across dirs, the first-scanned dir's mapping must not be overwritten.
	writeSensor(base / "hwmon0", 2, "pkg", "40000");
	writeSensor(base / "hwmon1", 2, "pkg", "90000");

	std::map<std::string, std::string> out;
	// Emulate the accumulation order explicitly so the test does not depend on
	// directory-iteration ordering.
	oal::hwmon_detail::scanHwmonDir(base / "hwmon0", "temp", out);
	oal::hwmon_detail::scanHwmonDir(base / "hwmon1", "temp", out);

	REQUIRE(out.count("pkg") == 1);
	CHECK(out["pkg"] == (base / "hwmon0" / "temp2_input").string());
}

TEST_CASE("scanHwmonDir: malformed / missing pieces do not produce entries")
{
	TempDir tmp;
	auto hwmon = tmp.path() / "hwmon1";
	// Empty label file -> no entry.
	writeFile(hwmon / "temp2_label", "");
	writeFile(hwmon / "temp2_input", "50000\n");
	// Strict-name reject: temp_label (no digits) even with a temp?_input around.
	writeFile(hwmon / "temp_label", "bogus\n");

	std::map<std::string, std::string> out;
	oal::hwmon_detail::scanHwmonDir(hwmon, "temp", out);
	CHECK(out.empty());

	// A non-existent hwmon dir simply yields nothing (no crash).
	std::map<std::string, std::string> out2;
	oal::hwmon_detail::scanHwmonDir(tmp.path() / "no_such_dir", "temp", out2);
	CHECK(out2.empty());
	CHECK(oal::hwmon_detail::scanHwmonRoot(tmp.path() / "no_such_base", "temp", out2) == 0);
}

TEST_CASE("scanHwmonRoot: two distinct device roots do not cross-resolve")
{
	TempDir tmpA;
	TempDir tmpB;
	auto baseA = tmpA.path() / "hwmon";
	auto baseB = tmpB.path() / "hwmon";
	writeSensor(baseA / "hwmon0", 2, "pkg", "50000");
	writeSensor(baseB / "hwmon0", 2, "pkg", "77000");

	std::map<std::string, std::string> outA;
	std::map<std::string, std::string> outB;
	oal::hwmon_detail::scanHwmonRoot(baseA, "temp", outA);
	oal::hwmon_detail::scanHwmonRoot(baseB, "temp", outB);

	REQUIRE(outA.count("pkg") == 1);
	REQUIRE(outB.count("pkg") == 1);
	// Each root resolves only within its own tree.
	CHECK(outA["pkg"].rfind(baseA.string(), 0) == 0);
	CHECK(outB["pkg"].rfind(baseB.string(), 0) == 0);
	CHECK(outA["pkg"] != outB["pkg"]);
}

TEST_CASE("scanHwmonDir: prefix is parameterized (matches a non-temp subsystem)")
{
	// The traversal is not hard-wired to "temp"; verify it resolves another
	// prefix so the helper stays reusable for other hwmon subsystems.
	TempDir tmp;
	auto hwmon = tmp.path() / "hwmon1";
	writeFile(hwmon / "in0_label", "vccgt\n");
	writeFile(hwmon / "in0_input", "950\n");
	// A temp sensor must NOT be picked up when scanning with prefix "in".
	writeSensor(hwmon, 2, "pkg", "50000");

	std::map<std::string, std::string> out;
	oal::hwmon_detail::scanHwmonDir(hwmon, "in", out);

	REQUIRE(out.count("vccgt") == 1);
	CHECK(out["vccgt"] == (hwmon / "in0_input").string());
	CHECK(out.count("pkg") == 0);
}
