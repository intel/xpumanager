/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "version_report.h"
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <driver.h>
#include <dynload.h>
#include <module_info.h>
#include "cmds.h" // progName
#include "utility/compat/format.h"
#include "xpum_build_info.h"

namespace {

// clang-format off
constexpr int LABEL_WIDTH        = 20; // "Interposed (LD_PRELOAD):" aside, the widest label fits in 20
constexpr int COMPONENT_WIDTH    = 18;
// clang-format on

constexpr std::string_view NOT_AVAILABLE = "N/A";
// Placeholder written by scripts/gen_build_info.py when git metadata is unavailable.
constexpr std::string_view UNKNOWN_FIELD = "unknown";

/**
 * @brief Renders one "  <label>  <value>" line, or nothing for an empty value.
 *
 * Empty means the field could not be determined; printing "Commit date:" with
 * nothing after it is worse than omitting the line.
 *
 * @param[in] label  Field label including its colon, e.g. "Git commit:".
 * @param[in] value  Field value.
 *
 * @retval line  The aligned, newline-terminated line.
 * @retval ""    @p value is empty.
 */
[[nodiscard]] std::string field(std::string_view label, std::string_view value)
{
	if (value.empty()) {
		return {};
	}
	return xpum::compat::format("  {0:<{1}} {2}\n", label, LABEL_WIDTH, value);
}

} // namespace

std::string formatEpochUtc(std::int64_t epoch)
{
	if (epoch < 0) {
		return {};
	}
	const std::chrono::sys_seconds when{std::chrono::seconds{epoch}};
	return xpum::compat::format("{:%Y-%m-%d %H:%M:%S} UTC", when);
}

VersionReport collectHostVersionReport()
{
	return VersionReport{
		.version = XPUM_BUILD_VERSION,
		.describe = XPUM_BUILD_DESCRIBE,
		.commit = XPUM_BUILD_COMMIT,
		.commitDate = formatEpochUtc(XPUM_BUILD_COMMIT_EPOCH),
		.buildTimestamp = formatEpochUtc(XPUM_BUILD_TIMESTAMP_EPOCH),
		.buildType = XPUM_BUILD_TYPE,
		.platform = XPUM_BUILD_PLATFORM,
		.loader = std::nullopt, // filled by addDriverVersionInfo()
		// Resolve the loader through one of its own symbols: which file the dynamic
		// loader actually picked is not something a version number can reveal. This
		// is a dlsym/GetProcAddress lookup, so it works before zeInit as well.
		.loaderPath = modulePathForSymbol(resolveL0Sym("zeInit")),
		.interposed = interposedModules(),
	};
}

void addDriverVersionInfo(VersionReport &report, driver &sm) { report.loader = sm.getLoaderComponent(); }

std::string formatVersionReport(const VersionReport &report)
{
	return formatHostVersionReport(report) + formatDriverVersionReport(report);
}

std::string formatHostVersionReport(const VersionReport &report)
{
	const std::string headline = report.describe.empty() || report.describe == UNKNOWN_FIELD
									 ? std::string{report.version}
									 : xpum::compat::format("{} ({})", report.version, report.describe);
	std::string out = xpum::compat::format("{} version {}\n\n", progName, headline);

	out += "Build:\n";
	out += field("Git commit:", report.commit);
	out += field("Commit date:", report.commitDate);
	out += field("Built:", report.buildTimestamp);
	out += field("Build type:", report.buildType);
	out += field("Platform:", report.platform);

	out += "\nRuntime:\n";
	if (report.loaderPath.has_value()) {
		out += field("Loader path:", *report.loaderPath);
	}
	if (!report.interposed.empty()) {
		// Worth its own section: a preloaded library can replace driver symbols
		// without changing any version number reported above.
		out += "  Interposed (LD_PRELOAD):\n";
		for (const auto &entry : report.interposed) {
			out += xpum::compat::format("    {}\n", entry);
		}
	}
	return out;
}

std::string formatDriverVersionReport(const VersionReport &report)
{
	// Continues the Runtime block opened by formatHostVersionReport().
	if (!report.loader.has_value()) {
		return field("Level Zero:", NOT_AVAILABLE);
	}
	const LoaderComponent &l = *report.loader;
	return "  Level Zero:\n" +
		   xpum::compat::format("    {0:<{1}} {2:<10} (spec {3})\n", l.name, COMPONENT_WIDTH, l.version, l.specVersion);
}
