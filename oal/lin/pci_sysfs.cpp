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

namespace {

constexpr std::string_view LIVE_PCI_DEVICES = "/sys/bus/pci/devices";

/** First line of a sysfs attribute, or nothing when it cannot be read. */
[[nodiscard]] std::optional<std::string> readLine(const std::filesystem::path &path)
{
	std::ifstream file(path);
	std::string line;
	if (!std::getline(file, line)) {
		return std::nullopt;
	}
	return line;
}

/** The PCIe generation of a sysfs link speed such as "32.0 GT/s PCIe". */
[[nodiscard]] std::optional<int> generationOfSpeed(std::string_view speed) noexcept
{
	constexpr std::array<std::pair<std::string_view, int>, 7> speeds{{
		{"2.5 GT/s", 1},
		{"5.0 GT/s", 2},
		{"8.0 GT/s", 3},
		{"16.0 GT/s", 4},
		{"32.0 GT/s", 5},
		{"64.0 GT/s", 6},
		{"128.0 GT/s", 7},
	}};
	for (const auto &[text, generation] : speeds) {
		if (speed.starts_with(text)) {
			return generation;
		}
	}
	return std::nullopt;
}

[[nodiscard]] std::optional<int> parseInt(std::string_view text) noexcept
{
	int value = 0;
	const char *const last = text.data() + text.size();
	const auto [end, ec] = std::from_chars(text.data(), last, value);
	if (ec != std::errc{} || end != last) {
		return std::nullopt;
	}
	return value;
}

/** The value of "<name> <count>" in an AER counter attribute, which lists one counter per line. */
[[nodiscard]] std::optional<uint64_t> aerCount(const std::filesystem::path &path, std::string_view name)
{
	std::ifstream file(path);
	std::string line;
	while (std::getline(file, line)) {
		const std::string_view text = line;
		if (text.size() > name.size() && text.starts_with(name) && text[name.size()] == ' ') {
			const std::string_view digits = text.substr(name.size() + 1);
			uint64_t value = 0;
			const char *const last = digits.data() + digits.size();
			const auto [end, ec] = std::from_chars(digits.data(), last, value);
			if (ec == std::errc{} && end == last) {
				return value;
			}
			return std::nullopt;
		}
	}
	return std::nullopt;
}

} // namespace

PcieLinkMax getRootPortMaxLink(std::string_view bdf, const std::filesystem::path &pciDevRoot)
{
	if (!isValidBdf(bdf)) {
		return {};
	}
	std::error_code ec;
	auto port = std::filesystem::canonical(pciDevRoot / bdf, ec);
	if (ec) {
		return {};
	}
	// Up the device tree: the root port is the last PCI function before the host bridge's
	// "pciDDDD:BB" directory.
	for (auto parent = port.parent_path(); isValidBdf(parent.filename().string()); parent = parent.parent_path()) {
		port = parent;
	}
	if (port.filename() == bdf) {
		return {}; // The function is itself on the root bus, with no port above it.
	}
	PcieLinkMax link;
	if (const auto speed = readLine(port / "max_link_speed")) {
		link.generation = generationOfSpeed(*speed);
	}
	if (const auto width = readLine(port / "max_link_width")) {
		link.width = parseInt(*width);
	}
	return link;
}

PcieLinkMax getRootPortMaxLink(std::string_view bdf) { return getRootPortMaxLink(bdf, LIVE_PCI_DEVICES); }

std::optional<PcieAerCounts> getPcieAerCounts(std::string_view bdf, const std::filesystem::path &pciDevRoot)
{
	if (!isValidBdf(bdf)) {
		return std::nullopt;
	}
	const auto dir = pciDevRoot / bdf;
	const auto correctable = aerCount(dir / "aer_dev_correctable", "TOTAL_ERR_COR");
	const auto nonFatal = aerCount(dir / "aer_dev_nonfatal", "TOTAL_ERR_NONFATAL");
	const auto fatal = aerCount(dir / "aer_dev_fatal", "TOTAL_ERR_FATAL");
	if (!correctable || !nonFatal || !fatal) {
		return std::nullopt;
	}
	return PcieAerCounts{.correctable = *correctable,
						 .nonFatal = *nonFatal,
						 .fatal = *fatal,
						 .replayRollover = aerCount(dir / "aer_dev_correctable", "Rollover").value_or(0),
						 .replayTimeout = aerCount(dir / "aer_dev_correctable", "Timeout").value_or(0)};
}

std::optional<PcieAerCounts> getPcieAerCounts(std::string_view bdf) { return getPcieAerCounts(bdf, LIVE_PCI_DEVICES); }

std::optional<PcieAtomicOps> getPcieAtomicOps(std::string_view bdf, const std::filesystem::path &pciDevRoot)
{
	if (!isValidBdf(bdf)) {
		return std::nullopt;
	}
	// Without CAP_SYS_ADMIN the kernel returns only the first 64 bytes, where no PCI Express
	// capability can be.
	constexpr std::size_t configSize = 256;
	std::array<unsigned char, configSize> config{};
	std::ifstream file(pciDevRoot / bdf / "config", std::ios::binary);
	file.read(reinterpret_cast<char *>(config.data()), configSize); // NOLINT: byte buffer for read()
	if (file.gcount() != static_cast<std::streamsize>(configSize)) {
		return std::nullopt;
	}
	const auto byteAt = [&config](std::size_t at) -> uint32_t { return config.at(at); };
	const auto dwordAt = [&byteAt](std::size_t at) -> uint32_t {
		return byteAt(at) | byteAt(at + 1) << 8 | byteAt(at + 2) << 16 | byteAt(at + 3) << 24;
	};
	constexpr std::size_t statusRegister = 0x06;
	constexpr uint32_t capabilitiesListBit = 0x10;
	constexpr std::size_t capabilitiesPointer = 0x34;
	constexpr uint32_t pciExpressCapabilityId = 0x10;
	constexpr std::size_t deviceCapabilities2 = 0x24;
	constexpr std::size_t deviceControl2 = 0x28;
	constexpr int maxCapabilities = 48; // a loop in a corrupt list cannot run forever
	if ((byteAt(statusRegister) & capabilitiesListBit) == 0) {
		return std::nullopt;
	}
	std::size_t at = byteAt(capabilitiesPointer) & ~0x3U;
	for (int i = 0; i < maxCapabilities && at >= 0x40 && at + deviceControl2 + 4 <= configSize; ++i) {
		if (byteAt(at) == pciExpressCapabilityId) {
			const uint32_t caps2 = dwordAt(at + deviceCapabilities2);
			const uint32_t control2 = dwordAt(at + deviceControl2);
			return PcieAtomicOps{.completer32 = (caps2 & (1U << 7)) != 0,
								 .completer64 = (caps2 & (1U << 8)) != 0,
								 .completer128Cas = (caps2 & (1U << 9)) != 0,
								 .requesterEnabled = (control2 & (1U << 6)) != 0};
		}
		at = byteAt(at + 1) & ~0x3U;
	}
	return std::nullopt;
}

std::optional<PcieAtomicOps> getPcieAtomicOps(std::string_view bdf) { return getPcieAtomicOps(bdf, LIVE_PCI_DEVICES); }
