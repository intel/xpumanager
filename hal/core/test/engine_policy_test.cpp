/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#undef INFO // doctest's INFO clashes with debug.h
#include "os.h"
#include <cstdlib>

TEST_CASE("sysmanEngineCountersAllowed: off by default on Linux")
{
	unsetenv("XPU_SMI_SYSMAN_ENGINE_UTIL");
	CHECK_FALSE(sysmanEngineCountersAllowed());
}

TEST_CASE("sysmanEngineCountersAllowed: XPU_SMI_SYSMAN_ENGINE_UTIL opts back in")
{
	setenv("XPU_SMI_SYSMAN_ENGINE_UTIL", "1", 1);
	CHECK(sysmanEngineCountersAllowed());
	unsetenv("XPU_SMI_SYSMAN_ENGINE_UTIL");
}
