/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <charconv>
#include <compare>
#include <cstdint>
#include <optional>
#include <string_view>
#include <system_error>

/** The class code of a PCI function, as its configuration space reports it. */
struct PciClassCode
{
	uint8_t baseClass = 0; ///< e.g. 0x03 for a display controller, 0x12 for a processing accelerator
	uint8_t subClass = 0;  ///< e.g. 0x00 for VGA compatible, 0x80 for other
	uint8_t progIf = 0;	   ///< Programming interface

	auto operator<=>(const PciClassCode &) const = default;
};

/**
 * @brief Splits a class code written as six hex digits, base class first.
 *
 * Both Linux sysfs ("030000" after its "0x") and the Windows compatible IDs ("CC_030000")
 * write it this way.
 *
 * @param[in] hexDigits Exactly six hex digits.
 *
 * @retval code    The base class, subclass and programming interface.
 * @retval nullopt @p hexDigits is not six hex digits.
 */
[[nodiscard]] inline std::optional<PciClassCode> parsePciClassCode(std::string_view hexDigits) noexcept
{
	constexpr std::size_t classDigits = 6;
	if (hexDigits.size() != classDigits) {
		return std::nullopt;
	}
	uint32_t value = 0;
	const char *const last = hexDigits.data() + hexDigits.size();
	const auto [end, ec] = std::from_chars(hexDigits.data(), last, value, 16);
	if (ec != std::errc{} || end != last) {
		return std::nullopt;
	}
	return PciClassCode{.baseClass = static_cast<uint8_t>((value >> 16) & 0xFFU),
						.subClass = static_cast<uint8_t>((value >> 8) & 0xFFU),
						.progIf = static_cast<uint8_t>(value & 0xFFU)};
}

/**
 * @brief Reads the class code of a PCI function from the OS.
 *
 * @param[in] bdf PCI address of the function, in canonical DDDD:BB:DD.F form.
 *
 * @retval code    The function's class code.
 * @retval nullopt @p bdf is malformed, names no PCI function, or the OS does not report its class.
 */
[[nodiscard]] std::optional<PciClassCode> getPciClassCode(std::string_view bdf);
