/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef XPUM_UTILITY_BDF_H
#define XPUM_UTILITY_BDF_H

#include <cctype>
#include <string>

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
 * @param bdf String to validate
 * @return true if @p bdf matches the canonical BDF format
 */
inline bool isValidBdf(const std::string &bdf)
{
	if (bdf.length() != 12) {
		return false;
	}
	const auto isHex = [](unsigned char c) { return std::isxdigit(c) != 0; };
	return isHex(bdf[0]) && isHex(bdf[1]) && isHex(bdf[2]) && isHex(bdf[3]) && bdf[4] == ':' && isHex(bdf[5]) &&
		   isHex(bdf[6]) && bdf[7] == ':' && isHex(bdf[8]) && isHex(bdf[9]) && bdf[10] == '.' && isHex(bdf[11]);
}

#endif // XPUM_UTILITY_BDF_H
