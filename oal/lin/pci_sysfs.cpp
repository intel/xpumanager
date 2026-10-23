/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "pci_sysfs.h"
#include "bdf.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#include <fcntl.h>
#include <unistd.h>

namespace {

/// A "0x"-prefixed hexadecimal sysfs attribute, the way the kernel prints PCI IDs and classes.
[[nodiscard]] std::optional<uint32_t> readHexAttribute(const std::filesystem::path &path)
{
	std::ifstream file(path);
	std::string line;
	if (!std::getline(file, line)) {
		return std::nullopt;
	}
	constexpr std::string_view hexPrefix = "0x";
	std::string_view digits = line;
	if (!digits.starts_with(hexPrefix)) {
		return std::nullopt;
	}
	digits.remove_prefix(hexPrefix.size());
	const char *const last = std::to_address(digits.end());
	uint32_t value = 0;
	const auto [end, ec] = std::from_chars(digits.data(), last, value, 16);
	if (ec != std::errc{} || end != last) {
		return std::nullopt;
	}
	return value;
}

} // namespace

std::optional<uint16_t> getPciSubsystemDeviceId(std::string_view bdf, const std::filesystem::path &pciDevRoot)
{
	if (!isValidBdf(bdf)) {
		return std::nullopt;
	}
	// The kernel prints this attribute as "0x%04x".
	const auto id = readHexAttribute(pciDevRoot / bdf / "subsystem_device");
	if (!id || *id > UINT16_MAX) {
		return std::nullopt;
	}
	return static_cast<uint16_t>(*id);
}

bool isIntelDisplayDevice(std::string_view bdf, const std::filesystem::path &pciDevRoot)
{
	constexpr uint32_t intelVendor = 0x8086;
	constexpr uint32_t displayBaseClass = 0x03; // PCI base class "display controller"
	if (!isValidBdf(bdf)) {
		return false;
	}
	const auto vendor = readHexAttribute(pciDevRoot / bdf / "vendor");
	const auto classCode = readHexAttribute(pciDevRoot / bdf / "class");
	return vendor == intelVendor && classCode && (*classCode >> 16) == displayBaseClass;
}

namespace {

// PCI configuration space layout (PCI Local Bus 3.0, PCIe Base 6.0 section 7.5.3).
constexpr std::size_t CFG_STATUS = 0x06;
constexpr std::uint16_t CFG_STATUS_CAP_LIST = 0x0010;
constexpr std::size_t CFG_CAP_POINTER = 0x34;
constexpr std::size_t CFG_FIRST_CAP = 0x40; // capabilities live after the standard header
constexpr std::uint8_t CAP_ID_PCIE = 0x10;
constexpr std::uint8_t PCIE_TYPE_RC_INTEGRATED_ENDPOINT = 0x9;
constexpr std::size_t CFG_LEGACY_SIZE = 256; // the capability list never leaves the first 256 bytes

} // namespace

std::optional<std::uint8_t> pcieDeviceType(std::span<const std::uint8_t> config) noexcept
{
	const auto word = [config](std::size_t offset) {
		return static_cast<std::uint16_t>(config[offset] | (config[offset + 1] << 8));
	};
	if (config.size() < CFG_FIRST_CAP || (word(CFG_STATUS) & CFG_STATUS_CAP_LIST) == 0) {
		return std::nullopt;
	}
	// Each capability is at least 4 bytes after the header, so a list longer than this loops.
	constexpr std::size_t maxCapabilities = (CFG_LEGACY_SIZE - CFG_FIRST_CAP) / 4;
	std::size_t offset = config[CFG_CAP_POINTER] & 0xFCU;
	for (std::size_t hop = 0; hop < maxCapabilities && offset >= CFG_FIRST_CAP && offset + 3 < config.size(); ++hop) {
		if (config[offset] == CAP_ID_PCIE) {
			return static_cast<std::uint8_t>((word(offset + 2) >> 4) & 0xFU);
		}
		offset = config[offset + 1] & 0xFCU;
	}
	return std::nullopt;
}

bool isRootComplexIntegrated(std::string_view bdf, const std::filesystem::path &pciDevRoot)
{
	if (!isValidBdf(bdf)) {
		return false;
	}
	// The PCIe capability names the device type. Reading past the first 64 bytes of config
	// space needs CAP_SYS_ADMIN, so an unprivileged caller falls back to the topology below.
	std::array<std::uint8_t, CFG_LEGACY_SIZE> config{};
	std::ifstream file(pciDevRoot / bdf / "config", std::ios::binary);
	file.read(reinterpret_cast<char *>(config.data()),
			  config.size()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): byte buffer
	const auto length = static_cast<std::size_t>(std::max<std::streamsize>(file.gcount(), 0));
	if (const auto type = pcieDeviceType(std::span{config}.first(length))) {
		return *type == PCIE_TYPE_RC_INTEGRATED_ENDPOINT;
	}
	// Without it: an integrated endpoint sits directly under its host bridge (pciDDDD:BB),
	// a discrete one behind a root port.
	std::error_code ec;
	const std::filesystem::path real = std::filesystem::canonical(pciDevRoot / bdf, ec);
	return !ec && real.parent_path().filename().string().starts_with("pci");
}

namespace {

/// Writes @p value to a sysfs attribute; a store reports its failure from write(), which std::ofstream would hide.
[[nodiscard]] std::error_code writeAttribute(const std::filesystem::path &attribute, std::string_view value)
{
	int fd = -1;
	do {
		fd = ::open(attribute.c_str(), O_WRONLY | O_CLOEXEC);
	} while (fd < 0 && errno == EINTR);
	if (fd < 0) {
		return {errno, std::generic_category()};
	}
	ssize_t written = -1;
	do {
		written = ::write(fd, value.data(), value.size());
	} while (written < 0 && errno == EINTR);
	const int writeErrno = errno;
	::close(fd);
	if (written < 0) {
		return {writeErrno, std::generic_category()};
	}
	// A sysfs store takes the whole value in one write; a short count did not apply it.
	if (static_cast<std::size_t>(written) != value.size()) {
		return std::make_error_code(std::errc::io_error);
	}
	return {};
}

} // namespace

std::error_code resetPciFunction(std::string_view bdf, const std::filesystem::path &pciBusRoot)
{
	if (!isValidBdf(bdf)) {
		return std::make_error_code(std::errc::invalid_argument);
	}
	const std::filesystem::path device = pciBusRoot / "devices" / bdf;
	std::error_code ec;
	if (!std::filesystem::exists(device / "reset", ec)) {
		return std::make_error_code(std::errc::not_supported);
	}

	// The driver must let go of the device and probe it again: a reset under a bound
	// driver leaves its software state, such as xe's wedged flag, in place.
	const std::filesystem::path bound = std::filesystem::read_symlink(device / "driver", ec);
	const std::filesystem::path driver = ec ? std::filesystem::path{} : pciBusRoot / "drivers" / bound.filename();
	if (!driver.empty()) {
		if (const auto unbound = writeAttribute(driver / "unbind", bdf)) {
			return unbound;
		}
	}
	const std::error_code reset = writeAttribute(device / "reset", "1");
	if (!driver.empty()) {
		// Rebind even after a failed reset, so the device is not left without its driver.
		if (const auto rebound = writeAttribute(driver / "bind", bdf); rebound && !reset) {
			return rebound;
		}
	}
	return reset;
}

std::optional<PciClassCode> getPciClassCode(std::string_view bdf, const std::filesystem::path &pciDevRoot)
{
	if (!isValidBdf(bdf)) {
		return std::nullopt;
	}
	std::ifstream file(pciDevRoot / bdf / "class");
	std::string line;
	if (!std::getline(file, line)) {
		return std::nullopt;
	}
	constexpr std::string_view hexPrefix = "0x";
	std::string_view digits = line;
	if (!digits.starts_with(hexPrefix)) {
		return std::nullopt;
	}
	digits.remove_prefix(hexPrefix.size());
	return parsePciClassCode(digits);
}

std::optional<PciClassCode> getPciClassCode(std::string_view bdf)
{
	return getPciClassCode(bdf, "/sys/bus/pci/devices");
}
