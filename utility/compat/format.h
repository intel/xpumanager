/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Compatibility shim for std::format (C++20, GCC 13+).
 *
 * GCC 12 ships std::format as incomplete/absent — use {fmt} as a drop-in.
 * Include this header and call xpum::compat::format(...) everywhere instead
 * of std::format(...) directly.
 */

#ifndef XPUM_UTILITY_COMPAT_FORMAT_H
#define XPUM_UTILITY_COMPAT_FORMAT_H

// __cpp_lib_format is defined when the compiler/stdlib ships a conforming
// std::format implementation (GCC 13+, Clang 14+ with libc++, MSVC 19.29+).
//
// Avoid <version> on Windows: the project's root-level VERSION file is
// reachable via -I.. and a case-insensitive filesystem would resolve
// #include <version> to it, producing a parse error. MSVC defines the
// library feature-test macros in <yvals_core.h>, which every standard header
// includes, so include one: without it, a file whose first include leads here
// would see no __cpp_lib_format and take the {fmt} branch.
#ifdef _MSC_VER
#include <cstddef>
#else
#include <version>
#endif

// The branch taken is announced as XPUM_COMPAT_FORMAT_STD. Headers that specialize a formatter
// must test that, not __cpp_lib_format: a header included later (e.g. <chrono> pulling in
// <format>) can define __cpp_lib_format after {fmt} was chosen. Define XPUM_COMPAT_FORMAT_USE_FMT
// to take the {fmt} branch even where std::format exists, e.g. to build the GCC 12 configuration
// with a newer compiler.
#if defined(__cpp_lib_format) && !defined(XPUM_COMPAT_FORMAT_USE_FMT)
#define XPUM_COMPAT_FORMAT_STD 1
#include <format>
namespace xpum::compat {
using std::format;
using std::format_string;
using std::vformat;
using std::make_format_args;

// Extract the underlying format string as a string_view for vformat().
// Deduce the CONCRETE format_string type as a plain `T` — writing the parameter
// as format_string<Args...> would make Args a non-deduced context (they are
// wrapped in type_identity_t) and force a consteval reconstruction. Take by
// const& so the consteval constructor is not re-triggered.
template <typename T> constexpr std::string_view fmt_view(const T &f) noexcept { return f.get(); }
} // namespace xpum::compat
#else
#include <fmt/format.h>
#include <fmt/chrono.h>  // chrono formatters not included by fmt/format.h unlike std::format
namespace xpum::compat {
using fmt::format;
using fmt::format_string;
using fmt::vformat;
using fmt::make_format_args;

// {fmt} 9's basic_format_string has no .get() (added in {fmt} 10); use its
// implicit string_view conversion instead, which works in both 9 and 10.
// See the std branch above for why the parameter is a plain deduced `T`.
template <typename T> constexpr fmt::string_view fmt_view(const T &f) noexcept { return f; }
} // namespace xpum::compat
#endif

#endif // XPUM_UTILITY_COMPAT_FORMAT_H
