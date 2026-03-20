/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef LOGGER_FORMATTERS_H
#define LOGGER_FORMATTERS_H

#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <type_traits>

// ── std::formatter extensions ─────────────────────────────────────────────────
//
// Two partial specialisations to provide formatting support for types not covered by
// C++20's std::format, but natively supported in C++23 (P2418R2).
//
// 1. Fixed-size char arrays (char[N])
//    - Enables formatting of C-style string buffers (device names, etc.)
//    - Example: char name[32] = "GPU-0"; std::format("{}", name);
//
// 2. Enumeration types
//    - Formats enums as their underlying integer value
//    - Example: enum class Status : int { OK = 0 }; std::format("{}", Status::OK);
//
// Note: Remove when migrating to C++23 or later.

namespace std // NOLINT(cert-dcl58-cpp) — extending std::formatter for external types
{

template <std::size_t N> struct formatter<char[N], char> : formatter<std::string_view, char> // NOLINT(cert-dcl58-cpp)
{
	auto format(const char (&arr)[N], std::format_context &ctx) const
	{
		return formatter<std::string_view, char>::format(std::string_view{arr, std::char_traits<char>::length(arr)},
														 ctx);
	}
};

template <typename EnumType>
	requires std::is_enum_v<EnumType>
struct formatter<EnumType, char> : formatter<std::underlying_type_t<EnumType>, char> // NOLINT(cert-dcl58-cpp)
{
	using UnderlyingType = std::underlying_type_t<EnumType>;
	auto format(EnumType val, std::format_context &ctx) const
	{
		return formatter<UnderlyingType, char>::format(static_cast<UnderlyingType>(val), ctx);
	}
};

} // namespace std

#endif /* LOGGER_FORMATTERS_H */
