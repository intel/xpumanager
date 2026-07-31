/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef LOGGER_FORMATTERS_H
#define LOGGER_FORMATTERS_H

#include <cstddef>
#include "utility/compat/format.h"
#include <string>
#include <string_view>
#include <type_traits>

// ── Formatter extensions ──────────────────────────────────────────────────────
//
// Two partial specialisations to provide formatting support for types not covered
// natively (available in C++23 / P2418R2, or {fmt} on older toolchains).
//
// 1. Fixed-size char arrays (char[N]) — C-style string buffers (device names, etc.)
// 2. Enumeration types — formatted as their underlying integer value
//
// Note: Remove when C++23 is the minimum standard.

#ifdef __cpp_lib_format

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

#else // !__cpp_lib_format — using {fmt}

namespace fmt // NOLINT(cert-dcl58-cpp) — extending fmt::formatter for external types
{

template <std::size_t N>
struct formatter<char[N]> : formatter<string_view>
{
	auto format(const char (&arr)[N], format_context &ctx) const
	{
		return formatter<string_view>::format(
			string_view{arr, std::char_traits<char>::length(arr)}, ctx);
	}
};

template <typename EnumType>
	requires std::is_enum_v<EnumType>
struct formatter<EnumType> : formatter<std::underlying_type_t<EnumType>>
{
	using UnderlyingType = std::underlying_type_t<EnumType>;
	auto format(EnumType val, format_context &ctx) const
	{
		return formatter<UnderlyingType>::format(static_cast<UnderlyingType>(val), ctx);
	}
};

} // namespace fmt

#endif // __cpp_lib_format

#endif /* LOGGER_FORMATTERS_H */
