/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// Shared between the request builder and parsePchResponse so neither side drifts.
namespace mkhi_pch {

constexpr uint8_t GROUP_ID = 0xF0U; // BUP_COMMON
constexpr uint8_t CMD = 0x12U;		// GET_PCH_INFO
constexpr uint32_t CMD_SHIFT = 8U;

constexpr size_t RESP_MIN_SIZE = 20U;
constexpr size_t RESP_VER_OFFSET = 12U;

constexpr uint32_t IS_RESP_BIT = 15U;
constexpr uint32_t RSVD_SHIFT = 16U;
constexpr uint32_t RESULT_SHIFT = 24U;

static_assert(RESP_VER_OFFSET + sizeof(uint32_t) <= RESP_MIN_SIZE);

} // namespace mkhi_pch

/** @brief Maps a 2-bit PchProdState value to its display string. */
[[nodiscard]] inline std::string_view pchProdStateName(uint32_t state) noexcept
{
	switch (state) {
	case 1U:
		return "Production ES";
	case 2U:
		return "Production QS";
	case 3U:
		return "Production PRQ";
	default:
		return "";
	}
}

/**
 * @brief Parses a raw MKHI GET_PCH_INFO response and returns the PchProdState.
 *
 * Pure function with no I/O — kept in the header so it can be unit-tested
 * without a MEI device.
 */
[[nodiscard]] inline std::optional<uint32_t> parsePchResponse(std::span<const uint8_t> response) noexcept
{
	static_assert(std::endian::native == std::endian::little, "MKHI wire format is little-endian; this host is not");

	if (response.size() < mkhi_pch::RESP_MIN_SIZE) {
		return std::nullopt;
	}

	auto readLE32 = [response](size_t offset) {
		std::array<uint8_t, sizeof(uint32_t)> bytes{};
		std::ranges::copy(response.subspan(offset, sizeof(uint32_t)), bytes.begin());
		return std::bit_cast<uint32_t>(bytes);
	};

	const auto hdr = readLE32(0);
	const auto ver = readLE32(mkhi_pch::RESP_VER_OFFSET);

	const auto group = static_cast<uint8_t>(hdr & 0xFFU);
	const auto respCmd = static_cast<uint8_t>((hdr >> mkhi_pch::CMD_SHIFT) & 0x7FU);
	const bool isResponse = ((hdr >> mkhi_pch::IS_RESP_BIT) & 0x1U) != 0U;
	const auto rsvd = static_cast<uint8_t>((hdr >> mkhi_pch::RSVD_SHIFT) & 0xFFU);
	const auto result = static_cast<uint8_t>((hdr >> mkhi_pch::RESULT_SHIFT) & 0xFFU);

	if (group != mkhi_pch::GROUP_ID || respCmd != mkhi_pch::CMD || !isResponse || rsvd != 0U || result != 0U) {
		return std::nullopt;
	}

	return ver & 0x3U;
}

/**
 * @brief Queries PCH production state via MEI/MKHI and returns the SKU type string.
 *
 * @param meiPath  Path to the MEI character device.
 * @return "Production ES", "Production QS", "Production PRQ", or "" on failure.
 */
[[nodiscard]] std::string getPchProdState(const std::string &meiPath);
