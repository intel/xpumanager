/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "privilege.h"
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <fcntl.h>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>

namespace {

// Bit numbers from <linux/capability.h>.
constexpr unsigned CAP_SYS_ADMIN_BIT = 21;
constexpr unsigned CAP_PERFMON_BIT = 38;

/// The effective capability set from /proc/self/status, read once.
[[nodiscard]] std::optional<uint64_t> effectiveCapabilities()
{
	static const std::optional<uint64_t> caps = []() -> std::optional<uint64_t> {
		std::ifstream status("/proc/self/status");
		constexpr std::string_view prefix = "CapEff:";
		std::string line;
		while (std::getline(status, line)) {
			std::string_view text = line;
			if (!text.starts_with(prefix)) {
				continue;
			}
			text.remove_prefix(prefix.size());
			text.remove_prefix(std::min(text.find_first_not_of(" \t"), text.size()));
			uint64_t value = 0;
			const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
			if (ec != std::errc{}) {
				return std::nullopt;
			}
			return value;
		}
		return std::nullopt;
	}();
	return caps;
}

[[nodiscard]] bool hasBit(uint64_t caps, unsigned bit) noexcept { return (caps >> bit & 1U) != 0; }

} // namespace

bool hasCapability(Capability cap) noexcept
{
	try {
		const auto caps = effectiveCapabilities();
		if (!caps) {
			return geteuid() == 0;
		}
		const bool sysAdmin = hasBit(*caps, CAP_SYS_ADMIN_BIT);
		return cap == Capability::SysAdmin ? sysAdmin : sysAdmin || hasBit(*caps, CAP_PERFMON_BIT);
	} catch (...) {
		return geteuid() == 0;
	}
}

bool canReadWrite(const std::string &path) noexcept
{
	return faccessat(AT_FDCWD, path.c_str(), R_OK | W_OK, AT_EACCESS) == 0;
}
