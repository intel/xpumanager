/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

/**
 * @file version_report_test.cpp
 * @brief Doctest-based unit tests for the version report renderers.
 *
 * Exercises the renderers against hand-built VersionReport values; the collectors
 * read the live build and driver, and are covered by the CLI validation suite
 * instead.
 */

// The clang-tidy CI step builds its compilation database from a default-options
// build, which excludes test targets, so doctest's include path is absent there and
// the file would fail to parse. Guard the body so that configuration lints an empty
// translation unit instead of reporting a hard error.
#if __has_include(<doctest/doctest.h>)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

// INFO may be defined by ze_api / level-zero headers; doctest also defines it.
// Undefine before including project headers to avoid the macro clash.
#ifdef INFO
#undef INFO
#endif

// progName is an extern defined by the CLI executable; provide a stub for tests. Its
// type is fixed by that extern declaration in cmds.h, so it cannot be made const.
#include <string>
std::string progName = "test"; // NOLINT(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)

#include "version_report.h"
#include <driver.h>

#include <optional>

namespace {

/**
 * @brief Builds a fully populated report; individual tests mutate the fields they cover.
 *
 * @retval VersionReport  Every field set, with no interposed libraries.
 */
[[nodiscard]] VersionReport sampleReport()
{
	return VersionReport{
		.version = "2.2.0",
		.describe = "internal-2026.40.1-1-gdeadbee",
		.commit = "deadbeefcafe",
		.commitDate = "2026-09-26 09:14:02 UTC",
		.buildTimestamp = "2026-09-28 14:02:11 UTC",
		.buildType = "release",
		.platform = "linux/x86_64",
		.loader = LoaderComponent{.name = "loader", .version = "1.33.1", .specVersion = "1.18"},
		.loaderPath = "/usr/lib/libze_loader.so.1",
		.interposed = {},
	};
}

} // namespace

TEST_CASE("the report prints exactly the agreed field set")
{
	// Pins the whole layout, not just a few labels: the field set was trimmed in
	// review to what users cannot get elsewhere, so an added line is as much a
	// regression as a missing one.
	VersionReport report = sampleReport();
	report.interposed = {"/usr/lib/libigc.so.2"};

	CHECK(formatVersionReport(report) == "test version 2.2.0 (internal-2026.40.1-1-gdeadbee)\n"
										 "\n"
										 "Build:\n"
										 "  Git commit:          deadbeefcafe\n"
										 "  Commit date:         2026-09-26 09:14:02 UTC\n"
										 "  Built:               2026-09-28 14:02:11 UTC\n"
										 "  Build type:          release\n"
										 "  Platform:            linux/x86_64\n"
										 "\n"
										 "Runtime:\n"
										 "  Loader path:         /usr/lib/libze_loader.so.1\n"
										 "  Interposed (LD_PRELOAD):\n"
										 "    /usr/lib/libigc.so.2\n"
										 "  Level Zero:\n"
										 "    loader             1.33.1     (spec 1.18)\n");
}

TEST_CASE("timestamps render as readable UTC, and only when available")
{
	CHECK(formatEpochUtc(1700000000) == "2023-11-14 22:13:20 UTC");
	// The epoch itself is a real time: SOURCE_DATE_EPOCH=0 is a valid reproducible build.
	CHECK(formatEpochUtc(0) == "1970-01-01 00:00:00 UTC");
	CHECK(formatEpochUtc(-1).empty());
}

TEST_CASE("the headline carries git describe, or the bare version without git metadata")
{
	// describe --dirty is the only remaining trace of local edits, via its suffix.
	VersionReport dirty = sampleReport();
	dirty.describe = "internal-2026.40.1-1-gdeadbee-dirty";
	CHECK(formatVersionReport(dirty).find("test version 2.2.0 (internal-2026.40.1-1-gdeadbee-dirty)\n") !=
		  std::string::npos);

	// A source-tarball build has no git metadata; "(unknown)" would be noise.
	VersionReport noGit = sampleReport();
	noGit.describe = "unknown";
	noGit.commit = "unknown";
	CHECK(formatVersionReport(noGit).find("test version 2.2.0\n") != std::string::npos);
}

TEST_CASE("fields and sections that could not be determined are omitted")
{
	VersionReport report = sampleReport();
	report.commitDate = "";
	report.loaderPath = std::nullopt;

	const std::string out = formatVersionReport(report);
	CHECK(out.find("Commit date:") == std::string::npos);
	CHECK(out.find("Loader path:") == std::string::npos);
	// Empty collections drop their whole section rather than printing a bare heading.
	CHECK(out.find("Interposed") == std::string::npos);
	// Fields that are present still render.
	CHECK(out.find("Build type:") != std::string::npos);
}

TEST_CASE("interposition and an unavailable loader are called out")
{
	// A preloaded library can replace driver symbols without changing any version
	// number, so its absence from the report would make the report misleading.
	VersionReport preloaded = sampleReport();
	preloaded.interposed = {"/usr/lib/libigc.so.2.42.0+35147.21751"};
	const std::string out = formatVersionReport(preloaded);
	CHECK(out.find("Interposed (LD_PRELOAD):") != std::string::npos);
	CHECK(out.find("/usr/lib/libigc.so.2.42.0+35147.21751") != std::string::npos);

	VersionReport noLoader = sampleReport();
	noLoader.loader = std::nullopt;
	const std::string degraded = formatVersionReport(noLoader);
	CHECK(degraded.find("Level Zero:") != std::string::npos);
	CHECK(degraded.find("N/A") != std::string::npos);
}

TEST_CASE("the host and driver halves of the report do not overlap")
{
	// The host half is printed before Level Zero is touched, so everything in it has
	// to be knowable without a driver; the driver half must not repeat it.
	const VersionReport report = sampleReport();

	const std::string host = formatHostVersionReport(report);
	CHECK(host.find("Git commit:") != std::string::npos);
	CHECK(host.find("Loader path:") != std::string::npos);
	CHECK(host.find("Level Zero") == std::string::npos);

	const std::string driver = formatDriverVersionReport(report);
	CHECK(driver.find("Level Zero:") != std::string::npos);
	CHECK(driver.find("Build:") == std::string::npos);
	CHECK(driver.find("Git commit:") == std::string::npos);
	CHECK(driver.find("Loader path:") == std::string::npos);
}

#else // !__has_include(<doctest/doctest.h>)

#include <cstdio>

/**
 * @brief Fails the test run when the file was built without doctest.
 *
 * Only the lint configuration above should ever reach this branch: meson declares
 * doctest a required dependency of every test target. If a misconfigured build does
 * compile it, fail with a clear message rather than leaving an object file with no
 * main() and an opaque link error.
 *
 * @retval 1  Always.
 */
int main()
{
	// Best effort: the exit status reports the failure even if this message cannot.
	static_cast<void>(std::fputs("version_report_test: doctest was not on the include path\n", stderr));
	return 1;
}

#endif // __has_include(<doctest/doctest.h>)
