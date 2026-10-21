/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
// doctest defines INFO(expr) for test context; undef it so debug.h (pulled in
// via power.h) can define INFO(fmt, ...) for log-level gating.
#undef INFO

#include "power.h"

#include <array>

namespace {

zes_power_properties_t domain(bool onSubdevice, int32_t defaultLimit, int32_t minLimit, int32_t maxLimit)
{
	zes_power_properties_t p{};
	p.onSubdevice = onSubdevice ? 1 : 0;
	p.defaultLimit = defaultLimit;
	p.minLimit = minLimit;
	p.maxLimit = maxLimit;
	return p;
}

} // namespace

// Tests for the pure choice that power::getLimitBounds() makes among the enumerated power domains.

TEST_CASE("cardLimitBoundsOf: reads the default, minimum and maximum of a device-level domain")
{
	const std::array props{domain(false, 190000, 1000, 380000)};

	const auto bounds = power::cardLimitBoundsOf(props);
	REQUIRE(bounds.has_value());
	CHECK(bounds->defaultMw == 190000U);
	CHECK(bounds->minMw == 1000U);
	CHECK(bounds->maxMw == 380000U);
}

TEST_CASE("cardLimitBoundsOf: no domains gives nothing") { CHECK_FALSE(power::cardLimitBoundsOf({}).has_value()); }

TEST_CASE("cardLimitBoundsOf: a device-level domain is chosen over an earlier sub-device one")
{
	const std::array props{domain(true, 50000, 500, 100000), domain(false, 190000, 1000, 380000)};

	const auto bounds = power::cardLimitBoundsOf(props);
	REQUIRE(bounds.has_value());
	CHECK(bounds->maxMw == 380000U);
}

TEST_CASE("cardLimitBoundsOf: only sub-device domains gives nothing")
{
	const std::array props{domain(true, 50000, 500, 100000), domain(true, 50000, 500, 100000)};
	CHECK_FALSE(power::cardLimitBoundsOf(props).has_value());
}

TEST_CASE("cardLimitBoundsOf: a domain with no positive maximum is skipped")
{
	const std::array props{domain(false, 190000, 1000, 0), domain(false, 150000, 2000, -1),
						   domain(false, 100000, 3000, 200000)};

	const auto bounds = power::cardLimitBoundsOf(props);
	REQUIRE(bounds.has_value());
	CHECK(bounds->defaultMw == 100000U);
	CHECK(bounds->minMw == 3000U);
	CHECK(bounds->maxMw == 200000U);
}

TEST_CASE("cardLimitBoundsOf: a limit the driver left unknown comes out as zero")
{
	const std::array props{domain(false, -1, -1, 380000)};

	const auto bounds = power::cardLimitBoundsOf(props);
	REQUIRE(bounds.has_value());
	CHECK(bounds->defaultMw == 0U);
	CHECK(bounds->minMw == 0U);
	CHECK(bounds->maxMw == 380000U);
}
