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
 * which the Linux user-mode driver happens to populate from the same srcversion
 * file today (it is what the smi banner prints). The requirement names the file,
 * not an API: zes_api.h defines that field as an "Installed driver version" that
 * "will be set to the string 'unknown' if this cannot be determined", so neither
 * its format nor its user-mode/kernel-mode meaning is guaranteed. Reading sysfs
 * keeps the value stable across user-mode driver changes, keeps it available when
 * Sysman cannot initialise, and lets the fallback paths be unit tested.
 */

#include "kernel_driver.h"
#include "bdf.h"
#include <array>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace fs = std::filesystem;

namespace {

// Intel GPU kernel modules, newest first. Only consulted when the device's own
// driver symlink cannot be read (no BDF given, or nothing bound to the device).
constexpr std::array<std::string_view, 2> GPU_MODULES{"xe", "i915"};

/// Strip surrounding whitespace, including the newline sysfs appends to every attribute.
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

/// Read the first line of a sysfs attribute. Returns "" on any failure, since a
/// missing or unreadable attribute and an empty one mean the same thing here.
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

std::string getKernelDriverSrcVersion(const std::string &bdf, const KernelDriverPaths &paths)
{
	const std::string driver = getKernelDriverName(bdf, paths);
	if (driver.empty()) {
		return {};
	}
	// Absent when the driver is compiled into the kernel rather than loaded as a
	// module, and when the module was built with CONFIG_MODULE_SRCVERSION_ALL off
	// and carries no MODULE_VERSION.
	return readAttribute(paths.moduleRoot / driver / "srcversion");
}
