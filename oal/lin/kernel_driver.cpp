/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Kernel-mode driver build identification, split out of lin.cpp so the sysfs
 * lookup (driver resolution, module fallback, attribute trimming) can be unit
 * tested against a fake /sys tree without linking the rest of the OS
 * abstraction layer.
 *
 * Read from sysfs rather than from Sysman's zes_device_properties_t.driverVersion,
 * which on Linux reports the user-mode driver and says nothing about the kernel
 * module: zes_api.h defines that field as an "Installed driver version" that "will
 * be set to the string 'unknown' if this cannot be determined", so neither its
 * format nor its user-mode/kernel-mode meaning is guaranteed. Reading sysfs keeps
 * the value stable across user-mode driver changes, keeps it available when Sysman
 * cannot initialise, and lets the lookup be unit tested.
 */

#include "kernel_driver.h"
#include "bdf.h"
#include <array>
#include <cctype>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace fs = std::filesystem;

namespace {

// Intel GPU kernel modules, newest first. Only consulted when the device's own
// driver symlink cannot be read (no BDF given, or nothing bound to the device).
constexpr std::array<std::string_view, 2> GPU_MODULES{"xe", "i915"};

/**
 * @brief Strips surrounding whitespace, including the newline sysfs appends to every attribute
 *
 * @param[in] s Raw attribute text
 * @return Trimmed copy, empty when @p s holds nothing but whitespace
 */
std::string trim(std::string_view s)
{
	constexpr std::string_view whitespace = " \t\n\r";
	const auto first = s.find_first_not_of(whitespace);
	if (first == std::string_view::npos) {
		return {};
	}
	const auto last = s.find_last_not_of(whitespace);
	return std::string{s.substr(first, last - first + 1)};
}

/**
 * @brief Reads the first line of a sysfs attribute
 *
 * @param[in] path Full path to the attribute file
 * @return Attribute contents, or an empty string on any failure, since a missing or
 *         unreadable attribute and an empty one mean the same thing here
 */
std::string readAttribute(const fs::path &path)
{
	std::ifstream file(path);
	if (!file) {
		return {};
	}
	std::string line;
	std::getline(file, line);
	return trim(line);
}

/**
 * @brief Pulls the backports release out of an out-of-tree driver's MODULE_VERSION string
 *
 * The backported Xe driver sets MODULE_VERSION to a sentence rather than a bare
 * version, e.g. "backported from (365b81808) using backports xeb_v7.1.4.31_260728.26
 * for 7.0.0-14-generic Kernel". Only the token after "using backports" names the
 * driver package, so that is what identifies the build.
 *
 * @param[in] version Raw MODULE_VERSION string
 * @return Backports release, or @p version unchanged when it carries no backports
 *         marker, since any other form is already a version string
 */
std::string extractBackportsRelease(const std::string &version)
{
	constexpr std::string_view marker = "using backports ";
	const auto markerPos = version.find(marker);
	if (markerPos == std::string::npos) {
		return version;
	}
	const auto tokenStart = markerPos + marker.size();
	const auto tokenEnd = version.find(' ', tokenStart);
	std::string release = version.substr(tokenStart, tokenEnd - tokenStart);
	// A marker with nothing after it says less than the sentence does.
	return release.empty() ? version : release;
}

} // namespace

std::string getBoundPciDriverName(const std::string &bdf, const KernelDriverPaths &paths)
{
	// Rejects the empty string along with anything that is not a canonical DDDD:BB:DD.F
	// address, so a malformed value cannot escape pciDevRoot when it is used as a path
	// component below. sysfs only ever names devices canonically, so nothing readable
	// is turned away.
	if (!isValidBdf(bdf)) {
		return {};
	}
	std::error_code ec;
	const fs::path target = fs::read_symlink(paths.pciDevRoot / bdf / "driver", ec);
	if (ec) {
		return {};
	}
	return target.filename().string();
}

std::string getKernelDriverName(const std::string &bdf, const KernelDriverPaths &paths)
{
	// The driver actually bound to this device is authoritative: a system can
	// have both xe and i915 loaded, each driving different GPUs.
	if (std::string bound = getBoundPciDriverName(bdf, paths); !bound.empty()) {
		return bound;
	}

	for (const auto module : GPU_MODULES) {
		std::error_code ec;
		if (fs::exists(paths.moduleRoot / module, ec) && !ec) {
			return std::string{module};
		}
	}
	return {};
}

std::string getKernelDriverModuleVersion(const std::string &bdf, const KernelDriverPaths &paths)
{
	const std::string driver = getKernelDriverName(bdf, paths);
	if (driver.empty()) {
		return {};
	}
	// The kernel creates this attribute only for modules built with a MODULE_VERSION
	// string. The out-of-tree/DKMS packaging sets one; the in-tree driver does not, so
	// the file is simply missing on a stock kernel.
	const std::string version = readAttribute(paths.moduleRoot / driver / "version");
	if (version.empty()) {
		return {};
	}
	return extractBackportsRelease(version);
}

/**
 * @brief Shortens a kernel release string to its version and distro build number
 *
 * Examples: "6.8.0-45-generic" -> "6.8.0-45",
 * "5.14.0-503.40.1.el9_5.x86_64" -> "5.14.0-503.40.1",
 * "7.1.0-lgci-xe-xe-internal-1441-5b9f3e73-nodebug+" -> "7.1.0"
 *
 * @param[in] release Kernel release string, as returned by uname -r
 * @return Short version, or an empty string if @p release doesn't start with one
 */
std::string getKernelReleaseShortVersion(const std::string &release)
{
	// Digits and dots up to the first other character. A distro build number right after
	// it is kept, since distros backport driver code into a kernel without changing its
	// base version: 6.8.0-45 and 6.8.0-90 carry different xe/i915 code, and RHEL's 5.14.0
	// carries xe/i915 from 6.x. Anything from the first letter on is a flavour or
	// architecture tag and is dropped, as is a trailing dot.
	constexpr std::string_view numeric = "0123456789.";
	auto end = release.find_first_not_of(numeric);
	if (end != std::string::npos && end > 0 && release[end] == '-' && end + 1 < release.size() &&
		std::isdigit(static_cast<unsigned char>(release[end + 1])) != 0) {
		end = release.find_first_not_of(numeric, end + 1);
	}
	std::string version = release.substr(0, end);
	while (!version.empty() && version.back() == '.') {
		version.pop_back();
	}
	if (version.empty() || version.front() == '.') {
		return {};
	}
	return version;
}

/**
 * @brief Returns the kernel version as the driver version for an in-tree GPU driver
 *
 * Only call this once getKernelDriverModuleVersion() has come back empty: an
 * out-of-tree driver names its own release, and that is not checked again here.
 *
 * If @p bdf is given, a driver must be bound to that device. If @p bdf is empty,
 * any loaded Intel GPU module is used.
 *
 * @param[in] bdf           PCI BDF address of the device, or "" for any loaded GPU module
 * @param[in] kernelRelease Kernel release string, as returned by uname -r
 * @param[in] paths         sysfs root paths (defaults to live sysfs; override in tests)
 * @return Short kernel version (e.g. "6.8.0-45"), or an empty string if no driver is found
 */
std::string getInTreeKernelDriverVersion(const std::string &bdf, const std::string &kernelRelease,
										 const KernelDriverPaths &paths)
{
	// No driver means nothing to attribute the kernel version to. A device given by BDF
	// must have a driver bound: a loaded module may be driving another GPU, not this one.
	const std::string driver = bdf.empty() ? getKernelDriverName(bdf, paths) : getBoundPciDriverName(bdf, paths);
	if (driver.empty()) {
		return {};
	}
	return getKernelReleaseShortVersion(kernelRelease);
}
