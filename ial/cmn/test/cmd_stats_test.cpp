/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
// doctest defines INFO(expr) for test context; undef it so debug.h (pulled in
// transitively via cmd_stats.h) can define INFO(fmt, ...) for log-level gating.
#undef INFO

#include "cmd_stats.h"

#include <string>

// Tests for StatsTextPrinter::formatPerFanValue, which renders a single per-fan
// table cell. The RPM fallback added a unit-suffix argument; these tests lock
// the new " RPM" rendering AND assert the existing percent rendering is
// unchanged (byte-for-byte identical to the previous inline formatting).

TEST_CASE("formatPerFanValue: percent rendering is unchanged")
{
	// Default unit "%", precision 0: identical to the prior "Fan <id>: <v>%".
	CHECK(StatsTextPrinter::formatPerFanValue(0, 50.0, 0, "%") == "Fan 0: 50%");
	CHECK(StatsTextPrinter::formatPerFanValue(1, 0.0, 0, "%") == "Fan 1: 0%");
	CHECK(StatsTextPrinter::formatPerFanValue(0, 100.0, 0, "%") == "Fan 0: 100%");
}

TEST_CASE("formatPerFanValue: RPM rendering uses a ' RPM' suffix")
{
	CHECK(StatsTextPrinter::formatPerFanValue(0, 1234.0, 0, " RPM") == "Fan 0: 1234 RPM");
	// A stopped fan (0 RPM) renders as an explicit reading, not N/A.
	CHECK(StatsTextPrinter::formatPerFanValue(0, 0.0, 0, " RPM") == "Fan 0: 0 RPM");
	CHECK(StatsTextPrinter::formatPerFanValue(2, 3000.0, 0, " RPM") == "Fan 2: 3000 RPM");
}

TEST_CASE("formatPerFanValue: precision controls decimal places for both units")
{
	CHECK(StatsTextPrinter::formatPerFanValue(0, 49.5, 1, "%") == "Fan 0: 49.5%");
	CHECK(StatsTextPrinter::formatPerFanValue(0, 1234.5, 1, " RPM") == "Fan 0: 1234.5 RPM");
}
