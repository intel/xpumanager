/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Binary storage unit strong type and user-defined literals.
 *
 * Usage:
 *   Bytes b = 4_MiB;
 *   uint64_t kb = b.kibibytes();           // 4096
 *   bool big = b > 1_GiB;                 // false
 *   Bytes total = 4_MiB + 512_KiB;
 *   Bytes scaled = 8 * 1_MiB;
 *
 *   // Wrapping a raw byte count from an external API:
 *   Bytes{ps.sharedSize}.kibibytes()
 *
 * UDL operators are only accessible via `using namespace xpum::units` or
 * `using xpum::units::operator""_MiB` etc.
 */

#ifndef XPUM_UTILITY_UNITS_H
#define XPUM_UTILITY_UNITS_H

#include <compare>
#include <cstdint>

namespace xpum::units {

class Bytes
{
	static constexpr uint64_t KIB = 1024ULL;

	uint64_t value;

public:
	constexpr explicit Bytes(uint64_t v) noexcept : value(v) {}

	[[nodiscard]] constexpr uint64_t bytes() const noexcept { return value; }
	[[nodiscard]] constexpr uint64_t kibibytes() const noexcept { return value / KIB; }
	[[nodiscard]] constexpr uint64_t mebibytes() const noexcept { return value / (KIB * KIB); }
	[[nodiscard]] constexpr uint64_t gibibytes() const noexcept { return value / (KIB * KIB * KIB); }

	constexpr auto operator<=>(const Bytes &) const noexcept = default;

	[[nodiscard]] constexpr Bytes operator+(Bytes rhs) const noexcept { return Bytes{value + rhs.value}; }
	[[nodiscard]] constexpr Bytes operator-(Bytes rhs) const noexcept { return Bytes{value - rhs.value}; }

	[[nodiscard]] friend constexpr Bytes operator*(uint64_t lhs, Bytes rhs) noexcept { return Bytes{lhs * rhs.value}; }
	[[nodiscard]] friend constexpr Bytes operator*(Bytes lhs, uint64_t rhs) noexcept { return Bytes{lhs.value * rhs}; }
};

// NOLINTBEGIN(google-runtime-int) — UDL parameter type is mandated by the standard
[[nodiscard]] constexpr Bytes operator""_KiB(unsigned long long n) noexcept { return Bytes{n * 1024ULL}; }
[[nodiscard]] constexpr Bytes operator""_MiB(unsigned long long n) noexcept { return Bytes{n * 1024ULL * 1024ULL}; }
[[nodiscard]] constexpr Bytes operator""_GiB(unsigned long long n) noexcept
{
	return Bytes{n * 1024ULL * 1024ULL * 1024ULL};
}
// NOLINTEND(google-runtime-int)

} // namespace xpum::units

#endif // XPUM_UTILITY_UNITS_H
