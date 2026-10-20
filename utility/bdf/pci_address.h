/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef XPUM_UTILITY_PCI_ADDRESS_H
#define XPUM_UTILITY_PCI_ADDRESS_H

#include "utility/compat/format.h"
#include <algorithm>
#include <charconv>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

/**
 * @brief A PCI address (domain, bus, device, function) as numbers
 *
 * Parse text once, at the edge, and compare, sort or format the value: every spelling of an
 * address (letter case, leading zeros in the domain) gives the same PciAddress, and toString()
 * always gives the same canonical text.
 */
struct PciAddress
{
	static constexpr std::uint8_t MAX_DEVICE = 31;
	static constexpr std::uint8_t MAX_FUNCTION = 7;

	std::uint32_t domain = 0;
	std::uint8_t bus = 0;
	std::uint8_t device = 0;   ///< 0 to MAX_DEVICE
	std::uint8_t function = 0; ///< 0 to MAX_FUNCTION

	/**
	 * @brief Parses DOMAIN:BB:DD.F
	 *
	 * The domain has 4 to 8 hex digits (Linux prints domains above 0xffff, e.g. behind VMD, with
	 * more than 4); bus and device have 2, the function 1. Hex digits may be of either case.
	 *
	 * @param[in] text Address to parse
	 *
	 * @retval address The parsed address
	 * @retval nullopt @p text is not a PCI address in that form, or its device or function is out
	 *                 of range
	 */
	[[nodiscard]] static std::optional<PciAddress> parse(std::string_view text) noexcept
	{
		constexpr std::size_t tailLength = 8; // ":BB:DD.F"
		constexpr std::size_t minDomainDigits = 4;
		constexpr std::size_t maxDomainDigits = 8;
		if (text.size() < minDomainDigits + tailLength || text.size() > maxDomainDigits + tailLength) {
			return std::nullopt;
		}
		const std::string_view domainText = text.substr(0, text.size() - tailLength);
		const std::string_view tail = text.substr(domainText.size());
		if (tail[0] != ':' || tail[3] != ':' || tail[6] != '.') {
			return std::nullopt;
		}
		const auto domainValue = parseHex(domainText);
		const auto busValue = parseHex(tail.substr(1, 2));
		const auto deviceValue = parseHex(tail.substr(4, 2));
		const auto functionValue = parseHex(tail.substr(7, 1));
		if (!domainValue || !busValue || !deviceValue || !functionValue || *deviceValue > MAX_DEVICE ||
			*functionValue > MAX_FUNCTION) {
			return std::nullopt;
		}
		return PciAddress{*domainValue, static_cast<std::uint8_t>(*busValue), static_cast<std::uint8_t>(*deviceValue),
						  static_cast<std::uint8_t>(*functionValue)};
	}

	/**
	 * @brief Formats the address canonically, as DDDD:BB:DD.F in lower case
	 *
	 * The domain has at least 4 digits and more only when its value needs them. The result
	 * contains only hex digits, ':' and '.', so it is safe in a file or object name.
	 *
	 * @retval The canonical text
	 */
	[[nodiscard]] std::string toString() const
	{
		return xpum::compat::format("{:04x}:{:02x}:{:02x}.{:x}", domain, static_cast<unsigned>(bus),
									static_cast<unsigned>(device), static_cast<unsigned>(function));
	}

	friend constexpr auto operator<=>(const PciAddress &, const PciAddress &) = default;

private:
	// All characters must be hex digits: std::from_chars alone would stop early, e.g. at a sign.
	[[nodiscard]] static std::optional<std::uint32_t> parseHex(std::string_view field) noexcept
	{
		constexpr auto isHex = [](char c) noexcept {
			return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
		};
		if (field.empty() || !std::ranges::all_of(field, isHex)) {
			return std::nullopt;
		}
		std::uint32_t value = 0;
		const auto [end, error] = std::from_chars(field.data(), field.data() + field.size(), value, 16);
		if (error != std::errc{} || end != field.data() + field.size()) {
			return std::nullopt;
		}
		return value;
	}
};

// "{}" formats a PciAddress canonically, through std::format or {fmt}, whichever the compat
// shim uses.
#ifdef XPUM_COMPAT_FORMAT_STD
template <> struct std::formatter<PciAddress, char> : std::formatter<std::string_view, char>
{
	auto format(const PciAddress &address, std::format_context &ctx) const
	{
		return std::formatter<std::string_view, char>::format(address.toString(), ctx);
	}
};
#else
template <> struct fmt::formatter<PciAddress> : fmt::formatter<fmt::string_view>
{
	auto format(const PciAddress &address, fmt::format_context &ctx) const
	{
		return fmt::formatter<fmt::string_view>::format(address.toString(), ctx);
	}
};
#endif

#endif // XPUM_UTILITY_PCI_ADDRESS_H
