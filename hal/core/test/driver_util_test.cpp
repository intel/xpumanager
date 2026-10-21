/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
// doctest defines INFO(expr) for test context; undef it so logger.h
// (pulled in via driver_util.h → device.h) can define INFO(fmt, ...).
#undef INFO

#include "driver_util.h"

#include <array>

using xpum::detail::deduplicateByIndex;
using xpum::detail::deviceLevelIndex;
using xpum::detail::parseUint32;

// ---------------------------------------------------------------------------
// parseUint32
// ---------------------------------------------------------------------------

TEST_CASE("parseUint32: valid decimal integers")
{
	CHECK(parseUint32("0") == std::optional<uint32_t>{0});
	CHECK(parseUint32("1") == std::optional<uint32_t>{1});
	CHECK(parseUint32("42") == std::optional<uint32_t>{42});
	CHECK(parseUint32("4294967295") == std::optional<uint32_t>{4294967295u}); // UINT32_MAX
}

TEST_CASE("parseUint32: empty or non-numeric input returns nullopt")
{
	CHECK(!parseUint32(""));
	CHECK(!parseUint32("abc"));
	CHECK(!parseUint32("0000:03:00.0")); // BDF address
	CHECK(!parseUint32("-1"));			 // negative
}

TEST_CASE("parseUint32: partial-numeric input returns nullopt")
{
	CHECK(!parseUint32("1abc")); // trailing non-digit
	CHECK(!parseUint32("1 "));	 // trailing space
	CHECK(!parseUint32(" 1"));	 // leading space
}

TEST_CASE("parseUint32: overflow returns nullopt")
{
	CHECK(!parseUint32("4294967296")); // UINT32_MAX + 1
	CHECK(!parseUint32("99999999999"));
}

// ---------------------------------------------------------------------------
// deduplicateByIndex
// ---------------------------------------------------------------------------

static devInfo makeDevInfo(uint32_t index)
{
	devInfo d{};
	d.index = index;
	return d;
}

TEST_CASE("deduplicateByIndex: no duplicates — list unchanged")
{
	std::vector<devInfo> list = {makeDevInfo(0), makeDevInfo(1), makeDevInfo(2)};
	deduplicateByIndex(list);
	REQUIRE(list.size() == 3);
	CHECK(list[0].index == 0);
	CHECK(list[1].index == 1);
	CHECK(list[2].index == 2);
}

TEST_CASE("deduplicateByIndex: duplicate removed, first occurrence kept")
{
	std::vector<devInfo> list = {makeDevInfo(0), makeDevInfo(0)};
	deduplicateByIndex(list);
	REQUIRE(list.size() == 1);
	CHECK(list[0].index == 0);
}

TEST_CASE("deduplicateByIndex: preserves insertion order")
{
	// "2,0,1,0" — second 0 removed, order 2-0-1 preserved
	std::vector<devInfo> list = {makeDevInfo(2), makeDevInfo(0), makeDevInfo(1), makeDevInfo(0)};
	deduplicateByIndex(list);
	REQUIRE(list.size() == 3);
	CHECK(list[0].index == 2);
	CHECK(list[1].index == 0);
	CHECK(list[2].index == 1);
}

TEST_CASE("deduplicateByIndex: empty list is a no-op")
{
	std::vector<devInfo> list;
	deduplicateByIndex(list);
	CHECK(list.empty());
}

// ---------------------------------------------------------------------------
// deviceLevelIndex
// ---------------------------------------------------------------------------

TEST_CASE("deviceLevelIndex: no handles gives nothing") { CHECK_FALSE(deviceLevelIndex({}).has_value()); }

TEST_CASE("deviceLevelIndex: the first handle that is not on a sub-device is chosen")
{
	const std::array<ze_bool_t, 4> flags{1, 1, 0, 0};
	CHECK(deviceLevelIndex(flags) == std::optional<std::size_t>{2});

	const std::array<ze_bool_t, 2> first{0, 1};
	CHECK(deviceLevelIndex(first) == std::optional<std::size_t>{0});
}

TEST_CASE("deviceLevelIndex: with only sub-device handles the first is chosen")
{
	const std::array<ze_bool_t, 3> flags{1, 1, 1};
	CHECK(deviceLevelIndex(flags) == std::optional<std::size_t>{0});
}

TEST_CASE("deviceLevelIndex: a single device-level handle is chosen")
{
	const std::array<ze_bool_t, 1> flags{0};
	CHECK(deviceLevelIndex(flags) == std::optional<std::size_t>{0});
}
