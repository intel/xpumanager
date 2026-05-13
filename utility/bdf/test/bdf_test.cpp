/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "bdf.h"
#include <string>
#include <string_view>

// ── Compile-time checks ───────────────────────────────────────────────────────
// isValidBdf is constexpr; these are verified at compile time with zero runtime
// cost.  Any failure here is a hard build error, not a test failure.
static_assert(isValidBdf("0000:4d:00.0"), "valid BDF must pass at compile time");
static_assert(!isValidBdf(""),             "empty string must fail at compile time");
static_assert(!isValidBdf("0000:4d:00."), "truncated BDF must fail at compile time");

TEST_CASE("Valid canonical BDF addresses")
{
	// lowercase hex
	CHECK(isValidBdf("0000:4d:00.0") == true);
	CHECK(isValidBdf("0000:00:01.0") == true);
	CHECK(isValidBdf("ffff:ff:ff.f") == true);
	// uppercase hex
	CHECK(isValidBdf("FFFF:FF:FF.F") == true);
	// mixed case
	CHECK(isValidBdf("AbCd:eF:01.a") == true);
}

TEST_CASE("Accepts std::string and std::string_view without a copy")
{
	// Implicit conversion from std::string to std::string_view
	const std::string s = "0000:4d:00.0";
	CHECK(isValidBdf(s) == true);

	// Explicit std::string_view
	const std::string_view sv = "0000:4d:00.0";
	CHECK(isValidBdf(sv) == true);

	// string_view of an invalid BDF
	CHECK(isValidBdf(std::string_view{"bad"}) == false);
}

TEST_CASE("Invalid BDF - wrong length")
{
	CHECK(isValidBdf("") == false);
	CHECK(isValidBdf("0000:4d:00.00") == false); // 13 chars
	CHECK(isValidBdf("0000:4d:00.")  == false);  // 11 chars
	CHECK(isValidBdf("000:4d:00.0")  == false);  // 11 chars
}

TEST_CASE("Invalid BDF - wrong separator positions")
{
	CHECK(isValidBdf("0000-4d:00.0") == false); // '-' instead of first ':'
	CHECK(isValidBdf("0000:4d-00.0") == false); // '-' instead of second ':'
	CHECK(isValidBdf("0000:4d:00:0") == false); // ':' instead of '.'
	CHECK(isValidBdf("0000.4d.00:0") == false); // '.' instead of ':'
}

TEST_CASE("Invalid BDF - non-hex characters")
{
	CHECK(isValidBdf("000g:4d:00.0") == false); // 'g' in domain
	CHECK(isValidBdf("0000:4x:00.0") == false); // 'x' in bus
	CHECK(isValidBdf("0000:4d:0z.0") == false); // 'z' in device
	CHECK(isValidBdf("0000:4d:00.g") == false); // 'g' in function
}

TEST_CASE("Invalid BDF - adversarial / path-traversal inputs")
{
	CHECK(isValidBdf("../.:..:.0.0") == false);
	CHECK(isValidBdf("000/../00.00") == false);
	CHECK(isValidBdf("            ") == false); // 12 spaces
	// Construct a string_view that explicitly includes embedded NUL bytes so the
	// view length is 12 but the content contains non-hex characters (\0).
	CHECK(isValidBdf(std::string_view("\0\0\0\0:\0\0:\0\0.\0", 12)) == false); // embedded NUL bytes
}
