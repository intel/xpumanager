/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <os.h>

#include "hwmon_internal.h"

#include <charconv>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

// Linux implementation of getHwmonLabelPaths() (declared in oal/os.h): discover
// a PCI device's hwmon temperature-style nodes by walking sysfs. This lives in
// the OS Abstraction Layer (oal) because it is platform-specific sysfs
// traversal; the portable read/bounds/decision logic stays in hal. oal must not
// depend on hal, so nothing here includes a hal header.
//
// Scope note: validated on a single-tile, single-fan Intel Arc Pro B70 (xe
// driver), where one device-level hwmon node carries the package ("pkg") and
// VRAM ("vram") temperatures. The prefix is parameterized so the same walk can
// serve other hwmon subsystems, but this PR only uses it with prefix "temp".

namespace oal
{
namespace hwmon_detail
{
namespace
{

// hwmon label/input filename suffixes and the hwmon subdir name prefix.
constexpr std::string_view kLabelSuffix = "_label";
constexpr std::string_view kInputSuffix = "_input";
constexpr std::string_view kHwmonDirPrefix = "hwmon";

/**
 * @brief Strict parse of a hwmon "<prefix><N>_label" filename.
 *
 * Accept ONLY the exact form <prefix> <one-or-more-digits> "_label" where the
 * "_label" suffix reaches the end of the filename. Rejects near-misses such as
 * "temperature_label" (non-digit body), "temp_label" (no digits),
 * "temp2_labelx" (suffix not at end) and "temp2_input" (wrong suffix). The
 * digits are parsed with std::from_chars and must consume the entire digit span.
 *
 * @param filename Directory entry name to test.
 * @param prefix Sensor-subsystem prefix (e.g. "temp").
 * @param idx Out-param receiving the parsed sensor index N on success.
 * @return true iff `filename` is a strict "<prefix><N>_label" name.
 */
bool parseLabelIndex(std::string_view filename, std::string_view prefix, unsigned int &idx)
{
	if (!filename.starts_with(prefix) || !filename.ends_with(kLabelSuffix)) {
		return false;
	}
	// The digit span sits strictly between the prefix and the "_label" suffix.
	if (filename.size() <= prefix.size() + kLabelSuffix.size()) {
		return false; // no room for at least one digit
	}
	const std::string_view digits =
		filename.substr(prefix.size(), filename.size() - prefix.size() - kLabelSuffix.size());
	unsigned int value = 0;
	const auto [ptr, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
	if (ec != std::errc() || ptr != digits.data() + digits.size()) {
		return false; // non-numeric body or trailing non-digit characters
	}
	idx = value;
	return true;
}

} // namespace

void scanHwmonDir(const std::filesystem::path &hwmonDir, std::string_view prefix,
				  std::map<std::string, std::string> &out)
{
	// Non-throwing directory_iterator: a missing/inaccessible path yields no
	// entries (via the error_code overload) instead of throwing.
	std::error_code ec;
	for (const auto &entry : std::filesystem::directory_iterator(hwmonDir, ec)) {
		const std::string entryName = entry.path().filename().string();
		unsigned int idx = 0;
		if (!parseLabelIndex(entryName, prefix, idx)) {
			continue;
		}
		std::ifstream lf(entry.path()); // the "<prefix><N>_label" file
		if (!lf.is_open()) {            // verify the label stream opens
			continue;
		}
		std::string label;
		if (!std::getline(lf, label) || label.empty()) {
			continue;
		}
		// Resolve the sibling "<prefix><N>_input" from the parsed index.
		const std::filesystem::path inputPath =
			hwmonDir / (std::string(prefix) + std::to_string(idx) + std::string(kInputSuffix));
		std::ifstream vf(inputPath); // verify the input stream opens
		if (!vf.is_open()) {
			continue;
		}
		out.emplace(std::move(label), inputPath.string()); // first match wins
	}
}

int scanHwmonRoot(const std::filesystem::path &hwmonBase, std::string_view prefix,
				  std::map<std::string, std::string> &out)
{
	// A device can expose more than one hwmonN directory and the labelled node is
	// not guaranteed to be the first; walk ALL hwmon* subdirs and accumulate.
	std::error_code ec;
	int visited = 0;
	for (const auto &entry : std::filesystem::directory_iterator(hwmonBase, ec)) {
		if (!entry.path().filename().string().starts_with(kHwmonDirPrefix)) {
			continue;
		}
		++visited;
		scanHwmonDir(entry.path(), prefix, out);
	}
	return visited;
}

} // namespace hwmon_detail
} // namespace oal

std::map<std::string, std::string> getHwmonLabelPaths(const std::string &pciBdf, const std::string &subsystemPrefix)
{
	const std::filesystem::path hwmonBase = std::filesystem::path("/sys/bus/pci/devices") / pciBdf / "hwmon";
	std::map<std::string, std::string> out;
	oal::hwmon_detail::scanHwmonRoot(hwmonBase, subsystemPrefix, out);
	return out;
}
