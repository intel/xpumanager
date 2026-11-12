/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "../text.h"

using xpum::text::iequal;

TEST_SUITE("xpum::text::iequal")
{
	TEST_CASE("strings that differ only in ASCII letter case are equal")
	{
		CHECK(iequal("warning", "warning"));
		CHECK(iequal("warning", "WARNING"));
		CHECK(iequal("Gpu.Temp", "gpu.temp"));
		CHECK(iequal("AZ", "az")); // both ends of the folded range
		CHECK(iequal("", ""));
	}

	TEST_CASE("different strings, or different lengths, are not equal")
	{
		CHECK_FALSE(iequal("ok", "okay"));
		CHECK_FALSE(iequal("ok", " ok"));
		CHECK_FALSE(iequal("failed", "fail"));
		CHECK_FALSE(iequal("ok", ""));
	}

	TEST_CASE("only ASCII letters are folded")
	{
		// '@' and '[' sit right before 'A' and right after 'Z'; folding them would make them match '`' and '{'.
		CHECK_FALSE(iequal("@", "`"));
		CHECK_FALSE(iequal("[", "{"));
		CHECK_FALSE(iequal("\xC3\x89", "\xC3\xA9")); // UTF-8 E-acute vs e-acute
	}

	TEST_CASE("usable in constant expressions")
	{
		static_assert(iequal("CRITICAL", "critical"));
		static_assert(!iequal("critical", "critica"));
	}
}
