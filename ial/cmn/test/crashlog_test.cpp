/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifdef INFO
#undef INFO
#endif

#include <string>
std::string progName = "test";

#include "cmd_crashlog.h"

#ifdef INFO
#undef INFO
#endif

using Action = cmdCrashlog::Action;

// The iclg command-building and output-parsing helpers now live in the OS layer
// and are covered by oal/lin/test/crashlog_lin_test.cpp.

TEST_SUITE("cmd_crashlog helpers")
{
	TEST_CASE("verbForAction maps each action to its iclg verb")
	{
		CHECK(std::string(cmdCrashlog::verbForAction(Action::Enable)) == "enable");
		CHECK(std::string(cmdCrashlog::verbForAction(Action::Disable)) == "disable");
		CHECK(std::string(cmdCrashlog::verbForAction(Action::Trigger)) == "trigger");
		CHECK(std::string(cmdCrashlog::verbForAction(Action::Clear)) == "clear");
		CHECK(std::string(cmdCrashlog::verbForAction(Action::Extract)) == "extract");
		CHECK(std::string(cmdCrashlog::verbForAction(Action::None)).empty());
	}
}
