/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
// doctest defines INFO(expr) for test context; undef it so debug.h (pulled in
// via frequency.h) can define INFO(fmt, ...) for log-level gating.
#undef INFO

#include "frequency.h"

#include <array>
#include <cmath>
#include <limits>

namespace {

zes_freq_properties_t propsOf(zes_freq_domain_t type, double min, double max)
{
	zes_freq_properties_t p{};
	p.type = type;
	p.min = min;
	p.max = max;
	return p;
}

} // namespace

// Tests for the pure selection that frequency::getMinFreqForDomain() and getMaxFreqForDomain() apply to
// the properties of every enumerated handle. A handle of another domain, and a figure the driver left
// at zero, must never be picked.

TEST_CASE("minFrequencyOf: picks the lowest positive minimum of the domain")
{
	const std::array props{propsOf(ZES_FREQ_DOMAIN_GPU, 400.0, 2850.0), propsOf(ZES_FREQ_DOMAIN_GPU, 300.0, 2850.0),
						   propsOf(ZES_FREQ_DOMAIN_MEMORY, 100.0, 9500.0)};

	const auto lowest = frequency::minFrequencyOf(props, ZES_FREQ_DOMAIN_GPU);
	REQUIRE(lowest.has_value());
	CHECK(*lowest == doctest::Approx(300.0));
}

TEST_CASE("maxFrequencyOf: picks the highest positive maximum of the domain")
{
	const std::array props{propsOf(ZES_FREQ_DOMAIN_GPU, 300.0, 2000.0), propsOf(ZES_FREQ_DOMAIN_GPU, 300.0, 2850.0),
						   propsOf(ZES_FREQ_DOMAIN_MEMORY, 100.0, 9500.0)};

	const auto highest = frequency::maxFrequencyOf(props, ZES_FREQ_DOMAIN_GPU);
	REQUIRE(highest.has_value());
	CHECK(*highest == doctest::Approx(2850.0));
}

TEST_CASE("min and maxFrequencyOf: no properties gives nothing")
{
	CHECK_FALSE(frequency::minFrequencyOf({}, ZES_FREQ_DOMAIN_GPU).has_value());
	CHECK_FALSE(frequency::maxFrequencyOf({}, ZES_FREQ_DOMAIN_GPU).has_value());
}

TEST_CASE("min and maxFrequencyOf: a domain with no handle gives nothing")
{
	const std::array props{propsOf(ZES_FREQ_DOMAIN_GPU, 300.0, 2850.0)};
	CHECK_FALSE(frequency::minFrequencyOf(props, ZES_FREQ_DOMAIN_MEDIA).has_value());
	CHECK_FALSE(frequency::maxFrequencyOf(props, ZES_FREQ_DOMAIN_MEDIA).has_value());
}

TEST_CASE("min and maxFrequencyOf: figures left at zero or below are ignored")
{
	const std::array props{propsOf(ZES_FREQ_DOMAIN_GPU, 0.0, 0.0), propsOf(ZES_FREQ_DOMAIN_GPU, -1.0, -1.0),
						   propsOf(ZES_FREQ_DOMAIN_GPU, 350.0, 2600.0)};

	CHECK(*frequency::minFrequencyOf(props, ZES_FREQ_DOMAIN_GPU) == doctest::Approx(350.0));
	CHECK(*frequency::maxFrequencyOf(props, ZES_FREQ_DOMAIN_GPU) == doctest::Approx(2600.0));

	const std::array unknown{propsOf(ZES_FREQ_DOMAIN_MEDIA, 0.0, 0.0)};
	CHECK_FALSE(frequency::minFrequencyOf(unknown, ZES_FREQ_DOMAIN_MEDIA).has_value());
	CHECK_FALSE(frequency::maxFrequencyOf(unknown, ZES_FREQ_DOMAIN_MEDIA).has_value());
}

TEST_CASE("min and maxFrequencyOf: a single handle is its own minimum and maximum")
{
	const std::array props{propsOf(ZES_FREQ_DOMAIN_MEDIA, 200.0, 1500.0)};
	CHECK(*frequency::minFrequencyOf(props, ZES_FREQ_DOMAIN_MEDIA) == doctest::Approx(200.0));
	CHECK(*frequency::maxFrequencyOf(props, ZES_FREQ_DOMAIN_MEDIA) == doctest::Approx(1500.0));
}

// Level Zero reports a frequency it does not know as a negative number.

TEST_CASE("knownFrequencyOf: a reading of zero or more is known")
{
	CHECK(*frequency::knownFrequencyOf(0.0) == doctest::Approx(0.0));
	CHECK(*frequency::knownFrequencyOf(1600.0) == doctest::Approx(1600.0));
}

TEST_CASE("knownFrequencyOf: a negative or non-numeric reading is unknown")
{
	CHECK_FALSE(frequency::knownFrequencyOf(-1.0).has_value());
	CHECK_FALSE(frequency::knownFrequencyOf(-0.001).has_value());
	CHECK_FALSE(frequency::knownFrequencyOf(std::numeric_limits<double>::quiet_NaN()).has_value());
}
