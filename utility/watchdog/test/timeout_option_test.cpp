/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "timeout_option.h"

#include <initializer_list>
#include <string>
#include <vector>

using namespace std::chrono_literals;
using xpum::watchdog::extractTimeoutOption;

namespace {

// Owns mutable copies so the parser sees a real `char *const` argv.
struct Argv
{
	std::vector<std::string> storage;
	std::vector<char *> pointers;

	Argv(std::initializer_list<const char *> args) : storage(args.begin(), args.end())
	{
		for (auto &arg : storage) {
			pointers.push_back(arg.data());
		}
	}

	[[nodiscard]] std::span<char *const> view() const { return pointers; }
};

[[nodiscard]] xpum::watchdog::TimeoutOption parse(std::initializer_list<const char *> args)
{
	const Argv argv{args};
	return extractTimeoutOption(argv.view());
}

} // namespace

TEST_CASE("absent: default timeout, arguments untouched")
{
	const auto opt = parse({"xpu-smi", "stats", "-d", "0"});
	CHECK_FALSE(opt.error);
	CHECK(opt.timeout == xpum::watchdog::DEFAULT_TIMEOUT);
	CHECK(opt.remaining == std::vector<std::string>{"xpu-smi", "stats", "-d", "0"});
}

TEST_CASE("separate value before the subcommand")
{
	const auto opt = parse({"xpu-smi", "--timeout", "5", "stats", "-d", "0"});
	CHECK_FALSE(opt.error);
	CHECK(opt.timeout == 5s);
	CHECK(opt.remaining == std::vector<std::string>{"xpu-smi", "stats", "-d", "0"});
}

TEST_CASE("joined value after the subcommand")
{
	const auto opt = parse({"xpu-smi", "stats", "-d", "0", "--timeout=120"});
	CHECK_FALSE(opt.error);
	CHECK(opt.timeout == 120s);
	CHECK(opt.remaining == std::vector<std::string>{"xpu-smi", "stats", "-d", "0"});
}

TEST_CASE("zero and the maximum are accepted")
{
	CHECK(parse({"xpu-smi", "--timeout=0"}).timeout == 0s);
	CHECK(parse({"xpu-smi", "--timeout", "86400"}).timeout == xpum::watchdog::MAX_TIMEOUT);
}

TEST_CASE("last occurrence wins and every occurrence is removed")
{
	const auto opt = parse({"xpu-smi", "--timeout=5", "discovery", "--timeout", "7"});
	CHECK_FALSE(opt.error);
	CHECK(opt.timeout == 7s);
	CHECK(opt.remaining == std::vector<std::string>{"xpu-smi", "discovery"});
}

TEST_CASE("argv[0] is never treated as the option")
{
	const auto opt = parse({"--timeout=5"});
	CHECK_FALSE(opt.error);
	CHECK(opt.timeout == xpum::watchdog::DEFAULT_TIMEOUT);
	CHECK(opt.remaining == std::vector<std::string>{"--timeout=5"});
}

TEST_CASE("an option that only starts with --timeout is left alone")
{
	const auto opt = parse({"xpu-smi", "--timeouts", "3"});
	CHECK_FALSE(opt.error);
	CHECK(opt.remaining == std::vector<std::string>{"xpu-smi", "--timeouts", "3"});
}

TEST_CASE("malformed values are errors")
{
	for (const char *bad : {"abc", "", "-1", "86401", "5s", "1.5", " 5", "99999999999999999999"}) {
		CAPTURE(bad);
		const auto joined = std::string{"--timeout="} + bad;
		const auto opt = parse({"xpu-smi", joined.c_str(), "discovery"});
		REQUIRE(opt.error);
		CHECK(opt.error->find("--timeout") != std::string::npos);
		CHECK(opt.error->find(std::string{"got '"} + bad + "'") != std::string::npos);
	}
}

TEST_CASE("missing value is an error")
{
	const auto opt = parse({"xpu-smi", "discovery", "--timeout"});
	REQUIRE(opt.error);
	CHECK(opt.error->find("--timeout") != std::string::npos);
}
