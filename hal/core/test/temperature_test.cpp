/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
// doctest defines INFO(expr) for test context; undef it so debug.h (pulled in
// via temperature.h) can define INFO(fmt, ...) for log-level gating.
#undef INFO

#include "temperature.h"

#include <cmath>
#include <limits>

// Tests for temperature::mr4CodeToCelsius(), which converts a JEDEC LPDDR5
// MR4 thermal refresh code (0..7) to the max-of-range Celsius value. Values
// outside [0, 7], non-finite values, and non-integer values must be returned
// unchanged so non-LPDDR5 paths and already-converted readings are not
// disturbed.

TEST_CASE("mr4CodeToCelsius: maps each MR4 code to JEDEC max-of-range")
{
	CHECK(temperature::mr4CodeToCelsius(0.0) == doctest::Approx(0.0));
	CHECK(temperature::mr4CodeToCelsius(1.0) == doctest::Approx(45.0));
	CHECK(temperature::mr4CodeToCelsius(2.0) == doctest::Approx(85.0));
	CHECK(temperature::mr4CodeToCelsius(3.0) == doctest::Approx(95.0));
	CHECK(temperature::mr4CodeToCelsius(4.0) == doctest::Approx(105.0));
	CHECK(temperature::mr4CodeToCelsius(5.0) == doctest::Approx(110.0));
	CHECK(temperature::mr4CodeToCelsius(6.0) == doctest::Approx(125.0));
	CHECK(temperature::mr4CodeToCelsius(7.0) == doctest::Approx(125.0));
}

TEST_CASE("mr4CodeToCelsius: values outside [0, 7] pass through unchanged")
{
	CHECK(temperature::mr4CodeToCelsius(-1.0) == doctest::Approx(-1.0));
	CHECK(temperature::mr4CodeToCelsius(8.0) == doctest::Approx(8.0));
	CHECK(temperature::mr4CodeToCelsius(42.5) == doctest::Approx(42.5));
	CHECK(temperature::mr4CodeToCelsius(125.0) == doctest::Approx(125.0));
}

TEST_CASE("mr4CodeToCelsius: non-integer values in [0, 7] pass through unchanged")
{
	// An already-converted Celsius reading that happens to fall inside the
	// MR4 numeric range (e.g. 3.5 C) must not be re-mapped.
	CHECK(temperature::mr4CodeToCelsius(0.5) == doctest::Approx(0.5));
	CHECK(temperature::mr4CodeToCelsius(3.5) == doctest::Approx(3.5));
	CHECK(temperature::mr4CodeToCelsius(6.999) == doctest::Approx(6.999));
}

TEST_CASE("mr4CodeToCelsius: integers represented with small FP error are still mapped")
{
	// Tolerance is 1e-9; values within that of an integer code count as that
	// code.
	CHECK(temperature::mr4CodeToCelsius(3.0 + 1e-12) == doctest::Approx(95.0));
	CHECK(temperature::mr4CodeToCelsius(3.0 - 1e-12) == doctest::Approx(95.0));
}

TEST_CASE("mr4CodeToCelsius: non-finite inputs pass through unchanged")
{
	const double nan = std::numeric_limits<double>::quiet_NaN();
	const double inf = std::numeric_limits<double>::infinity();

	// NaN compares unequal to itself, so verify with std::isnan / std::isinf
	// rather than equality (which would also trip -Wfloat-equal).
	CHECK(std::isnan(temperature::mr4CodeToCelsius(nan)));
	CHECK(std::isinf(temperature::mr4CodeToCelsius(inf)));
	CHECK(temperature::mr4CodeToCelsius(inf) > 0.0);
	CHECK(std::isinf(temperature::mr4CodeToCelsius(-inf)));
	CHECK(temperature::mr4CodeToCelsius(-inf) < 0.0);
}
