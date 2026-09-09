/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Tests for parsePchResponse() and pchProdStateName().  Both are header-only,
 * so no MEI hardware or METEE linkage is needed.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "pch_prod_state.h"

#include <array>
#include <cstdint>

// Verify the noexcept contract at compile time.
static_assert(noexcept(parsePchResponse(std::span<const uint8_t>{})));
static_assert(noexcept(pchProdStateName(0U)));

namespace {

// Derived from mkhi_pch constants so a protocol constant change breaks tests here too.
constexpr uint32_t VALID_HDR = static_cast<uint32_t>(mkhi_pch::GROUP_ID) |
							   (static_cast<uint32_t>(mkhi_pch::CMD) << mkhi_pch::CMD_SHIFT) |
							   (1U << mkhi_pch::IS_RESP_BIT);

// std::bit_cast matches how the parser deserializes, keeping the round-trip symmetric.
constexpr std::array<uint8_t, mkhi_pch::RESP_MIN_SIZE> makeResp(uint32_t hdr, uint32_t pchVersion)
{
	const auto h = std::bit_cast<std::array<uint8_t, sizeof(uint32_t)>>(hdr);
	const auto v = std::bit_cast<std::array<uint8_t, sizeof(uint32_t)>>(pchVersion);
	return {{
		h[0],  h[1],  h[2],	 h[3],	// header
		0x00U, 0x00U, 0x00U, 0x00U, // pchDeviceId
		0x00U,						// pchStep
		0x00U,						// pchRevision
		0x00U, 0x00U,				// reserved
		v[0],  v[1],  v[2],	 v[3],	// pchVersion
		0x00U, 0x00U, 0x00U, 0x00U, // pchReplacement
	}};
}

} // namespace

TEST_SUITE("pchProdStateName — string mapping")
{
	TEST_CASE("state 0 maps to empty string") { CHECK(pchProdStateName(0U).empty()); }

	TEST_CASE("state 1 maps to \"Production ES\"") { CHECK(pchProdStateName(1U) == "Production ES"); }

	TEST_CASE("state 2 maps to \"Production QS\"") { CHECK(pchProdStateName(2U) == "Production QS"); }

	TEST_CASE("state 3 maps to \"Production PRQ\"") { CHECK(pchProdStateName(3U) == "Production PRQ"); }

	TEST_CASE("out-of-range state maps to empty string")
	{
		CHECK(pchProdStateName(4U).empty());
		CHECK(pchProdStateName(0xFFFFFFFFU).empty());
	}
}

TEST_SUITE("parsePchResponse — valid responses")
{
	TEST_CASE("state 1 (ES) is returned for pchVersion = 0x00000001")
	{
		const auto result = parsePchResponse(makeResp(VALID_HDR, 0x00000001U));
		REQUIRE(result.has_value());
		CHECK(*result == 1U);
	}

	TEST_CASE("state 2 (QS) is returned for pchVersion = 0x00000002")
	{
		const auto result = parsePchResponse(makeResp(VALID_HDR, 0x00000002U));
		REQUIRE(result.has_value());
		CHECK(*result == 2U);
	}

	TEST_CASE("state 3 (PRQ) is returned for pchVersion = 0x00000003")
	{
		const auto result = parsePchResponse(makeResp(VALID_HDR, 0x00000003U));
		REQUIRE(result.has_value());
		CHECK(*result == 3U);
	}

	TEST_CASE("state 0 is returned for pchVersion = 0x00000000")
	{
		const auto result = parsePchResponse(makeResp(VALID_HDR, 0x00000000U));
		REQUIRE(result.has_value());
		CHECK(*result == 0U);
	}

	TEST_CASE("upper bits of pchVersion are masked; only bits [1:0] matter")
	{
		const auto result = parsePchResponse(makeResp(VALID_HDR, 0xFFFFFFFFU));
		REQUIRE(result.has_value());
		CHECK(*result == 3U);
	}

	TEST_CASE("a response longer than 20 bytes is accepted")
	{
		constexpr size_t oversizedLen = mkhi_pch::RESP_MIN_SIZE + 12U;
		std::array<uint8_t, oversizedLen> buf{};
		std::ranges::copy(makeResp(VALID_HDR, 0x00000003U), buf.begin());
		const auto result = parsePchResponse(buf);
		REQUIRE(result.has_value());
		CHECK(*result == 3U);
	}
}

TEST_SUITE("parsePchResponse — buffer size checks")
{
	TEST_CASE("empty buffer returns nullopt") { CHECK_FALSE(parsePchResponse({}).has_value()); }

	TEST_CASE("19-byte buffer (one byte short) returns nullopt")
	{
		const auto full = makeResp(VALID_HDR, 0x00000003U);
		CHECK_FALSE(parsePchResponse(std::span<const uint8_t>{full}.first(19)).has_value());
	}

	TEST_CASE("exactly 20-byte buffer is accepted")
	{
		CHECK(parsePchResponse(makeResp(VALID_HDR, 0x00000003U)).has_value());
	}
}

TEST_SUITE("parsePchResponse — header field validation")
{
	TEST_CASE("wrong group_id returns nullopt")
	{
		constexpr uint32_t badHdr = (VALID_HDR & ~0xFFU) | 0x03U;
		CHECK_FALSE(parsePchResponse(makeResp(badHdr, 0x00000003U)).has_value());
	}

	TEST_CASE("wrong command echo returns nullopt")
	{
		constexpr uint32_t badHdr = (VALID_HDR & ~0x7F00U) | 0x0000U;
		CHECK_FALSE(parsePchResponse(makeResp(badHdr, 0x00000003U)).has_value());
	}

	TEST_CASE("is_response bit clear returns nullopt")
	{
		constexpr uint32_t badHdr = VALID_HDR & ~(1U << mkhi_pch::IS_RESP_BIT);
		CHECK_FALSE(parsePchResponse(makeResp(badHdr, 0x00000003U)).has_value());
	}

	TEST_CASE("non-zero reserved field returns nullopt")
	{
		constexpr uint32_t badHdr = VALID_HDR | (1U << mkhi_pch::RSVD_SHIFT);
		CHECK_FALSE(parsePchResponse(makeResp(badHdr, 0x00000003U)).has_value());
	}

	TEST_CASE("non-zero result code returns nullopt")
	{
		constexpr uint32_t badHdr = VALID_HDR | (1U << mkhi_pch::RESULT_SHIFT);
		CHECK_FALSE(parsePchResponse(makeResp(badHdr, 0x00000003U)).has_value());
	}

	TEST_CASE("all header fields wrong simultaneously returns nullopt")
	{
		CHECK_FALSE(parsePchResponse(makeResp(0xFFFFFFFFU, 0x00000003U)).has_value());
	}
}
