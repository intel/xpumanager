/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef UTILITY_TEXT_H
#define UTILITY_TEXT_H

#include <algorithm>
#include <string_view>

namespace xpum::text {

/**
 * @brief Compares two strings, ignoring the case of ASCII letters.
 *
 * Only 'A'-'Z' are folded to 'a'-'z'; every other byte has to match exactly, so the result does not depend on the
 * C locale.
 *
 * @param[in] a First string.
 * @param[in] b Second string.
 * @return true if @p a and @p b have the same length and the same bytes once ASCII letters are folded.
 */
[[nodiscard]] constexpr bool iequal(std::string_view a, std::string_view b) noexcept
{
	constexpr auto fold = [](char c) noexcept { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
	return std::ranges::equal(a, b, {}, fold, fold);
}

} // namespace xpum::text

#endif // UTILITY_TEXT_H
