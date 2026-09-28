/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef VERSION_REPORT_H
#define VERSION_REPORT_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <driver.h>

/**
 * @brief Everything needed to identify a build and the stack it is running on.
 *
 * Build fields are std::string_view over the string literals in the generated
 * xpum_build_info.h, so they have static storage duration and outlive any report.
 */
struct VersionReport
{
	// Build provenance, fixed at compile time.
	std::string_view version;
	std::string_view describe; ///< git describe --dirty; its "-dirty" suffix flags local edits.
	std::string_view commit;
	// Not string_view: the generated header carries epoch seconds, and these are
	// rendered by formatEpochUtc() at collection time.
	std::string commitDate;
	std::string buildTimestamp;
	std::string_view buildType;
	std::string_view platform;

	// Runtime environment, discovered per invocation.
	std::optional<LoaderComponent> loader;
	std::optional<std::string> loaderPath;
	std::vector<std::string> interposed;
};

/**
 * @brief Renders epoch seconds as a human-readable UTC timestamp,
 *        e.g. "2026-10-05 19:26:15 UTC".
 *
 * ISO 8601 ordering, but with the space separator RFC 3339 permits instead of "T",
 * and "UTC" spelled out instead of "Z": both are opaque to anyone who does not
 * already know the notation, and this is read by people, not parsed.
 *
 * @param[in] epoch Seconds since the Unix epoch; negative means the value was
 *                  unavailable at build time (no git metadata). 0 is a real
 *                  timestamp, which SOURCE_DATE_EPOCH=0 legitimately produces.
 * @retval ""         @p epoch is negative, so the caller omits the field entirely.
 * @retval timestamp  "YYYY-MM-DD HH:MM:SS UTC" for any non-negative @p epoch.
 */
[[nodiscard]] std::string formatEpochUtc(std::int64_t epoch);

/**
 * @brief Collects everything that needs no Level Zero driver.
 *
 * Build provenance is compiled in and the runtime fields come from the OS and the
 * process image, so this cannot fail and needs no initialized driver: `-v` must
 * still identify the binary on a system whose driver stack is broken, which is
 * exactly when someone is most likely to run it.
 *
 * @retval VersionReport  Build fields and the host-side runtime fields filled in;
 *                        the driver-side fields are left for addDriverVersionInfo().
 */
[[nodiscard]] VersionReport collectHostVersionReport();

/**
 * @brief Fills the fields of @p report that are only meaningful once the driver is up.
 *
 * Every query degrades to an empty field rather than failing, so an uninitialized
 * or partly working driver yields a shorter report, not an error.
 *
 * @param[in,out] report Report to extend; its host-side fields are left untouched.
 * @param[in,out] sm     Initialized sysman driver, used for the loader query.
 */
void addDriverVersionInfo(VersionReport &report, driver &sm);

/**
 * @brief Renders @p report as an aligned, human-readable block.
 *
 * @param[in] report  Report to render; empty fields and sections are omitted.
 *
 * @retval text  The rendered block, newline-terminated.
 */
[[nodiscard]] std::string formatVersionReport(const VersionReport &report);

/**
 * @brief Renders the sections that need no driver: headline, Build, and the
 *        loader-path and interposition lines of Runtime.
 *
 * Split out so the CLI can emit them before touching Level Zero, where init can
 * fail or crash inside a broken driver. This is also the text of the banner that
 * opens every debug log.
 *
 * @param[in] report  Report from collectHostVersionReport().
 *
 * @retval text  The rendered block, newline-terminated.
 */
[[nodiscard]] std::string formatHostVersionReport(const VersionReport &report);

/**
 * @brief Renders the section that needs an initialized driver: the Level Zero
 *        loader version, continuing the Runtime block.
 *
 * Per-device driver versions are deliberately absent: `discovery` already reports
 * UMD Version, KMD Version and Kernel Version per device, in text and JSON.
 *
 * @param[in] report  Report after addDriverVersionInfo(); a missing loader renders
 *                    as "N/A".
 *
 * @retval text  The rendered block, newline-terminated.
 */
[[nodiscard]] std::string formatDriverVersionReport(const VersionReport &report);

#endif // VERSION_REPORT_H
