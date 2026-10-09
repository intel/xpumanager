/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "pci_sysfs.h"
#include "bdf.h"
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

std::optional<uint16_t> getPciSubsystemDeviceId(std::string_view bdf, const std::filesystem::path &pciDevRoot)
{
	if (!isValidBdf(bdf)) {
		return std::nullopt;
	}
	std::ifstream file(pciDevRoot / bdf / "subsystem_device");
	std::string line;
	if (!std::getline(file, line)) {
		return std::nullopt;
	}
	// The kernel prints this attribute as "0x%04x".
	constexpr std::string_view hexPrefix = "0x";
	std::string_view digits = line;
	if (!digits.starts_with(hexPrefix)) {
		return std::nullopt;
	}
	digits.remove_prefix(hexPrefix.size());
	const char *const last = std::to_address(digits.end());
	uint16_t id = 0;
	const auto [end, ec] = std::from_chars(digits.data(), last, id, 16);
	if (ec != std::errc{} || end != last) {
		return std::nullopt;
	}
	return id;
}
