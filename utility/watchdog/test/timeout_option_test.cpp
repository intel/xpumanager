/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "timeout_option.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <initializer_list>
#include <semaphore>
#include <string>
#include <string_view>
#include <thread>
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

TEST_CASE("absent: no timeout given, arguments untouched")
{
	const auto opt = parse({"xpu-smi", "stats", "-d", "0"});
	CHECK_FALSE(opt.error);
	CHECK(opt.timeout == std::nullopt);
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
	CHECK(opt.timeout == std::nullopt);
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

TEST_CASE("initialTimeout: the given --timeout, else the base")
{
	CHECK(xpum::watchdog::initialTimeout(parse({"xpu-smi", "discovery"})) == xpum::watchdog::BASE_TIMEOUT);
	CHECK(xpum::watchdog::initialTimeout(parse({"xpu-smi", "--timeout=5", "discovery"})) == 5s);
	CHECK(xpum::watchdog::initialTimeout(parse({"xpu-smi", "--timeout=0", "discovery"})) == 0s);
}

namespace {

constexpr auto ARMED_TIMEOUT = std::chrono::milliseconds{100};
// Generous: CI machines can stall a thread for far longer than the timeout.
constexpr auto EXPIRY_WAIT = std::chrono::seconds{5};

std::atomic<int> expiries{0};				  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<const char *> lastPhase{nullptr}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
std::binary_semaphore timedOut{0};			  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void recordExpiry(const xpum::watchdog::Expiry &expiry) noexcept
{
	expiries.fetch_add(1);
	lastPhase.store(expiry.phase);
	if (expiry.kind == xpum::watchdog::ExpiryKind::Timeout) {
		timedOut.release();
	}
}

void resetExpiries()
{
	expiries.store(0);
	lastPhase.store(nullptr);
	while (timedOut.try_acquire()) {
	}
}

} // namespace

TEST_CASE("applyDefaultTimeout: a given --timeout is kept and the GPUs are not counted")
{
	const auto opt = parse({"xpu-smi", "--timeout=5", "discovery"});
	xpum::watchdog::Watchdog dog{xpum::watchdog::initialTimeout(opt), recordExpiry};
	bool counted = false;
	xpum::watchdog::applyDefaultTimeout(dog, opt, [&counted] {
		counted = true;
		return std::size_t{8};
	});
	CHECK_FALSE(counted);
}

TEST_CASE("applyDefaultTimeout: a count that blocks ends in a timeout naming the phase")
{
	resetExpiries();
	const auto opt = parse({"xpu-smi", "discovery"});
	xpum::watchdog::Watchdog dog{ARMED_TIMEOUT, recordExpiry};
	xpum::watchdog::applyDefaultTimeout(dog, opt, [] {
		// Returns once the watchdog has fired, standing in for a sysfs read that never returns.
		const bool fired = timedOut.try_acquire_for(EXPIRY_WAIT);
		CHECK(fired);
		return std::size_t{8};
	});
	REQUIRE(lastPhase.load() != nullptr);
	CHECK(std::string_view{lastPhase.load()} == "counting GPUs");
}

TEST_CASE("applyDefaultTimeout: after the count, a stall longer than the armed timeout does not fire")
{
	resetExpiries();
	const auto opt = parse({"xpu-smi", "discovery"});
	xpum::watchdog::Watchdog dog{ARMED_TIMEOUT, recordExpiry};
	xpum::watchdog::applyDefaultTimeout(dog, opt, [] { return std::size_t{0}; });
	std::this_thread::sleep_for(3 * ARMED_TIMEOUT);
	CHECK(expiries.load() == 0);
}
