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

#include "cmd_amc.h"
#include "cmd_config.h"
#include "cmd_crashlog.h"
#include "cmd_discovery.h"
#include "cmd_dump.h"
#include "cmd_health.h"
#include "cmd_listpciinfo.h"
#include "cmd_log.h"
#include "cmd_topology.h"

#ifdef INFO
#undef INFO
#endif

#include <initializer_list>
#include <span>
#include <vector>

namespace {

// Owns the mutable char * tokens that commands receive as argv.
class Argv
{
	std::vector<std::string> tokens;
	std::vector<char *> ptrs;

public:
	Argv(std::initializer_list<const char *> list) : tokens(list.begin(), list.end())
	{
		for (auto &token : tokens) {
			ptrs.push_back(token.data());
		}
	}
	[[nodiscard]] std::span<char *const> view() const { return ptrs; }
};

} // namespace

// requiredDriver() is what the dispatcher consults before run(), so these
// cases cover the same selection runCli() makes.
TEST_SUITE("cmds::requiredDriver")
{
	TEST_CASE("device-facing commands default to the full driver")
	{
		const cmdDiscovery discovery;
		CHECK(discovery.requiredDriver(Argv{}.view()) == DriverMode::Full);
		CHECK(discovery.requiredDriver(Argv{"-d", "0", "-j"}.view()) == DriverMode::Full);
	}

	TEST_CASE("-h and --help never need the driver")
	{
		const cmdDiscovery discovery;
		CHECK(discovery.requiredDriver(Argv{"-h"}.view()) == DriverMode::None);
		CHECK(discovery.requiredDriver(Argv{"-d", "0", "--help"}.view()) == DriverMode::None);
		CHECK(cmdConfig{}.requiredDriver(Argv{"--reset", "-h"}.view()) == DriverMode::None);
	}

	TEST_CASE("command-owned help forms need no driver")
	{
		CHECK(cmdDump{}.requiredDriver(Argv{}.view()) == DriverMode::None);
		CHECK(cmdDump{}.requiredDriver(Argv{"help"}.view()) == DriverMode::None);
		CHECK(cmdHealth{}.requiredDriver(Argv{}.view()) == DriverMode::None);
		CHECK(cmdCrashlog{}.requiredDriver(Argv{}.view()) == DriverMode::None);
		CHECK(cmdTopology{}.requiredDriver(Argv{}.view()) == DriverMode::None);
	}

	TEST_CASE("the same commands need the driver once they have work to do")
	{
		CHECK(cmdDump{}.requiredDriver(Argv{"-d", "0", "--metrics", "0"}.view()) == DriverMode::Full);
		CHECK(cmdDump{}.requiredDriver(Argv{"-d", "0", "help"}.view()) == DriverMode::Full);
		CHECK(cmdHealth{}.requiredDriver(Argv{"-d", "0"}.view()) == DriverMode::Full);
		CHECK(cmdCrashlog{}.requiredDriver(Argv{"--extract", "-d", "0"}.view()) == DriverMode::Full);
		CHECK(cmdTopology{}.requiredDriver(Argv{"-m"}.view()) == DriverMode::Full);
	}

	TEST_CASE("commands that never touch Level Zero need no driver")
	{
		const Argv args{"-d", "0", "-j"};
		CHECK(cmdAmc{}.requiredDriver(args.view()) == DriverMode::None);
		CHECK(cmdListpciinfo{}.requiredDriver(args.view()) == DriverMode::None);
		CHECK(cmdLogs{}.requiredDriver(Argv{"-f", "logs.tar.gz"}.view()) == DriverMode::None);
	}

	TEST_CASE("config keeps the compute runtime off a device it is about to reset")
	{
		const cmdConfig config;
		CHECK(config.requiredDriver(Argv{"-d", "0", "--reset"}.view()) == DriverMode::SysmanOnly);
		CHECK(config.requiredDriver(Argv{"--reset", "-d", "0"}.view()) == DriverMode::SysmanOnly);
		CHECK(config.requiredDriver(Argv{"-d", "0"}.view()) == DriverMode::Full);
		CHECK(config.requiredDriver(Argv{"-d", "0", "--resetfrequencyrange"}.view()) == DriverMode::Full);
		CHECK(config.requiredDriver(Argv{"-d", "0000:03:00.0", "--coldreset"}.view()) == DriverMode::Full);
	}
}
