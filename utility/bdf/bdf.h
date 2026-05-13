/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef XPUM_UTILITY_BDF_H
#define XPUM_UTILITY_BDF_H

#include <string_view>

/**
 * @brief Validates that a string is a well-formed PCI BDF address
 *
 * The canonical BDF format is DDDD:BB:DD.F: 4 hex digits (domain), ':',
 * 2 hex digits (bus), ':', 2 hex digits (device), '.', 1 hex digit
 * (function) — exactly 12 characters.
 *
 * The format is identical on Linux and Windows, so this is a portable
 * pure-string check.  Useful both for sysfs path-traversal protection on
 * Linux and for early CLI argument validation on either platform.
 *
 * The hex digit check is locale-independent (explicit character ranges),
 * making the function safe to call in any locale context.
 *
 * Evaluates at compile time when @p bdf is a compile-time constant.
 *
 * @param bdf String to validate
 * @return true if @p bdf matches the canonical BDF format
 */
[[nodiscard]] constexpr bool isValidBdf(std::string_view bdf) noexcept
{
	if (bdf.size() != 12) {
		return false;
	}
	constexpr auto isHex = [](char c) constexpr noexcept {
		return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
	};
	return isHex(bdf[0]) && isHex(bdf[1]) && isHex(bdf[2]) && isHex(bdf[3]) && bdf[4] == ':' && isHex(bdf[5]) &&
		   isHex(bdf[6]) && bdf[7] == ':' && isHex(bdf[8]) && isHex(bdf[9]) && bdf[10] == '.' && isHex(bdf[11]);
}

#endif // XPUM_UTILITY_BDF_H
