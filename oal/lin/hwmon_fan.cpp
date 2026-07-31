/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <os.h>

#include "hwmon_fan_internal.h"

#include <charconv>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

// Linux implementation of getHwmonInputPaths() (declared in oal/os.h): discover
// a PCI device's hwmon fan nodes by walking sysfs. This lives in the OS
// Abstraction Layer (oal) because it is platform-specific sysfs traversal; the
// portable read/parse/decision logic stays in hal. oal must not depend on hal,
// so nothing here includes a hal header.
//
// Scope note: validated on the Intel Arc Pro B70 (xe driver), where Level Zero
// sysman enumerates no fan handle but the kernel publishes RPM through the PCI
// device's hwmon "fanN_input" nodes. Unlike temperature hwmon there is no
// "fanN_label"; fanN_input is enumerated directly and keyed by its zero-based
// index. The prefix is parameterized, but this PR only uses it with "fan".

namespace oal
{
namespace hwmon_fan_detail
{
namespace
{

// hwmon input filename suffix and the hwmon subdir name prefix.
constexpr std::string_view kInputSuffix = "_input";
constexpr std::string_view kHwmonDirPrefix = "hwmon";

/**
 * @brief Strict parse of a hwmon "<prefix><N>_input" filename to a zero-based index.
 *
 * Accept ONLY the exact form <prefix> <one-or-more-digits> "_input" where the
 * "_input" suffix reaches the end of the filename. hwmon fanN numbering is
 * 1-based, so N must be >= 1 and the returned index is (N - 1): "fan1_input" ->
 * 0. Rejects near-misses such as "fan_input" (no digits), "fanX_input" (non-digit
 * body), "fan1_label" (wrong suffix), "temp1_input" (wrong prefix),
 * "fan0_input" (invalid 1-based zero) and an N so large that (N - 1) would
 * overflow a uint32 index. The digits are parsed with std::from_chars, which
 * validates the whole span in one shot (res.ptr == end && res.ec == {}) and
 * flags out-of-range, so there is no separate manual digit loop.
 *
 * @param filename Directory entry name to test.
 * @param prefix Sensor-subsystem prefix (e.g. "fan").
 * @param zeroBasedIdx Out-param receiving the zero-based index on success.
 * @return true iff `filename` is a strict, in-range "<prefix><N>_input" name.
 */
bool parseInputIndex(std::string_view filename, std::string_view prefix, uint32_t &zeroBasedIdx)
{
	if (!filename.starts_with(prefix) || !filename.ends_with(kInputSuffix)) {
		return false;
	}
	// The digit span sits strictly between the prefix and the "_input" suffix.
	if (filename.size() <= prefix.size() + kInputSuffix.size()) {
		return false; // no room for at least one digit
	}
	const std::string_view digits =
		filename.substr(prefix.size(), filename.size() - prefix.size() - kInputSuffix.size());
	uint64_t value = 0;
	const auto [ptr, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
	if (ec != std::errc() || ptr != digits.data() + digits.size()) {
		return false; // non-numeric body, trailing junk, or from_chars out-of-range
	}
	if (value < 1) {
		return false; // hwmon fanN is 1-based; fan0_input is not a valid node
	}
	const uint64_t zeroBased = value - 1;
	if (zeroBased > static_cast<uint64_t>(UINT32_MAX)) {
		return false; // would overflow a uint32 fan index; never wrap and alias a fan
	}
	zeroBasedIdx = static_cast<uint32_t>(zeroBased);
	return true;
}

} // namespace

void scanHwmonInputDir(const std::filesystem::path &hwmonDir, std::string_view prefix,
					   std::map<uint32_t, std::string> &out)
{
	// Non-throwing directory_iterator: a missing/inaccessible path yields no
	// entries (via the error_code overload) instead of throwing.
	std::error_code ec;
	for (const auto &entry : std::filesystem::directory_iterator(hwmonDir, ec)) {
		const std::string entryName = entry.path().filename().string();
		uint32_t idx = 0;
		if (!parseInputIndex(entryName, prefix, idx)) {
			continue;
		}
		out.emplace(idx, entry.path().string()); // first match wins for a given index
	}
}

int scanHwmonInputRoot(const std::filesystem::path &hwmonBase, std::string_view prefix,
					   std::map<uint32_t, std::string> &out)
{
	// A device can expose more than one hwmonN directory and the fan node is not
	// guaranteed to be the first. Walk the hwmon* subdirs and accept the FIRST
	// one that actually yields a valid "<prefix>N_input" entry, so an unproductive
	// (e.g. temperature-only) node never shadows the real fan node.
	std::error_code ec;
	int visited = 0;
	for (const auto &entry : std::filesystem::directory_iterator(hwmonBase, ec)) {
		if (!entry.path().filename().string().starts_with(kHwmonDirPrefix)) {
			continue;
		}
		++visited;
		std::map<uint32_t, std::string> candidate;
		scanHwmonInputDir(entry.path(), prefix, candidate);
		if (!candidate.empty()) {
			out = std::move(candidate); // first hwmon node with a valid entry wins
			break;
		}
	}
	return visited;
}

} // namespace hwmon_fan_detail
} // namespace oal

std::map<uint32_t, std::string> getHwmonInputPaths(const std::string &pciBdf, const std::string &subsystemPrefix)
{
	const std::filesystem::path hwmonBase = std::filesystem::path("/sys/bus/pci/devices") / pciBdf / "hwmon";
	std::map<uint32_t, std::string> out;
	oal::hwmon_fan_detail::scanHwmonInputRoot(hwmonBase, subsystemPrefix, out);
	return out;
}
