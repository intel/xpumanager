/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "scope_exit.h"

TEST_CASE("ScopeExit executes callable on scope exit")
{
	int counter = 0;
	{
		ScopeExit guard{[&counter] { ++counter; }};
		CHECK(counter == 0);
	}
	CHECK(counter == 1);
}

TEST_CASE("ScopeExit executes on early return path")
{
	int counter = 0;
	auto fn = [&counter] {
		ScopeExit guard{[&counter] { ++counter; }};
		return; // early return
	};
	fn();
	CHECK(counter == 1);
}

TEST_CASE("ScopeExit executes exactly once")
{
	int counter = 0;
	{
		ScopeExit guard{[&counter] { ++counter; }};
		ScopeExit guard2{[&counter] { ++counter; }};
	}
	CHECK(counter == 2);
}

TEST_CASE("ScopeExit destructor is noexcept even if callable throws")
{
	// The destructor must not propagate exceptions — verify noexcept guarantee.
	CHECK(noexcept(std::declval<ScopeExit<decltype([] {})>>().~ScopeExit()));

	// Verify a throwing lambda does NOT terminate (exception is swallowed).
	bool ran = false;
	{
		ScopeExit guard{[&ran] {
			ran = true;
			throw std::runtime_error("should be swallowed");
		}};
	}
	CHECK(ran == true);
}

TEST_CASE("ScopeExit CTAD deduces type from lambda")
{
	// This test simply verifies that CTAD compiles without an explicit template argument.
	bool ran = false;
	{
		ScopeExit guard{[&ran] { ran = true; }};
	}
	CHECK(ran == true);
}
