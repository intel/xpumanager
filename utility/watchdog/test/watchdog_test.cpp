/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "watchdog.h"

#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <semaphore>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

using namespace std::chrono_literals;
using xpum::watchdog::Expiry;
using xpum::watchdog::ExpiryKind;
using xpum::watchdog::Watchdog;

static_assert(!std::is_constructible_v<xpum::watchdog::PhaseName, const char *>);
static_assert(std::is_constructible_v<xpum::watchdog::PhaseName, const char (&)[5]>);
static_assert(xpum::watchdog::DEFAULT_TIMEOUT == 30s);
static_assert(xpum::watchdog::pretimeoutAfter(30s) == 15s);
static_assert(xpum::watchdog::pretimeoutAfter(31s) == 15s);
static_assert(xpum::watchdog::pretimeoutAfter(1s) == 500ms);
static_assert(xpum::watchdog::pretimeoutAfter(0s) == 0ms);

namespace {

constexpr auto TIMEOUT = std::chrono::milliseconds{60};
// Generous: CI machines can stall a thread for far longer than the timeout.
constexpr auto WAIT = std::chrono::seconds{5};

// The handler is a plain function pointer, so it reports to whichever Recorder the current test
// installed. Each test destroys its Watchdog (joining the thread) before its Recorder.
struct Recorder
{
	std::mutex mutex;
	std::vector<Expiry> events;
	std::counting_semaphore<1024> pretimeout{0};
	std::counting_semaphore<1024> timeout{0};

	[[nodiscard]] std::vector<Expiry> snapshot()
	{
		std::scoped_lock lock{mutex};
		return events;
	}

	[[nodiscard]] int count(ExpiryKind kind)
	{
		std::scoped_lock lock{mutex};
		return static_cast<int>(std::ranges::count(events, kind, &Expiry::kind));
	}
};

std::atomic<Recorder *> recorder{nullptr};

void record(const Expiry &expiry) noexcept
{
	Recorder *r = recorder.load();
	{
		std::scoped_lock lock{r->mutex};
		r->events.push_back(expiry);
	}
	switch (expiry.kind) {
	case ExpiryKind::Pretimeout:
		r->pretimeout.release();
		break;
	case ExpiryKind::Timeout:
		r->timeout.release();
		break;
	}
}

struct Fixture
{
	Recorder rec;
	Fixture() { recorder.store(&rec); }
	~Fixture() { recorder.store(nullptr); }
	Fixture(const Fixture &) = delete;
	Fixture &operator=(const Fixture &) = delete;
	Fixture(Fixture &&) = delete;
	Fixture &operator=(Fixture &&) = delete;
};

[[nodiscard]] std::string format(const Expiry &expiry, std::string_view command)
{
	std::array<char, 512> buf{};
	return std::string{xpum::watchdog::formatMessage(expiry, command, buf)};
}

} // namespace

TEST_CASE_FIXTURE(Fixture, "fires a pretimeout warning, then one timeout, naming the current phase")
{
	const auto start = xpum::watchdog::Clock::now();
	Watchdog dog{TIMEOUT, record};
	const auto phase = xpum::watchdog::enter("driver init");

	REQUIRE(rec.timeout.try_acquire_for(WAIT));
	CHECK(xpum::watchdog::Clock::now() - start >= TIMEOUT);

	const auto events = rec.snapshot();
	REQUIRE(events.size() == 2);
	CHECK(events[0].kind == ExpiryKind::Pretimeout);
	CHECK(events[0].elapsed == xpum::watchdog::pretimeoutAfter(TIMEOUT));
	CHECK(events[0].remaining == TIMEOUT - xpum::watchdog::pretimeoutAfter(TIMEOUT));
	CHECK(std::string_view{events[0].phase} == "driver init");
	CHECK(events[1].kind == ExpiryKind::Timeout);
	CHECK(events[1].elapsed == TIMEOUT);
	CHECK(std::string_view{events[1].phase} == "driver init");

	// The thread stops after a timeout whose handler returns.
	CHECK_FALSE(rec.timeout.try_acquire_for(3 * TIMEOUT));
}

TEST_CASE_FIXTURE(Fixture, "keepalive prevents both the warning and the timeout")
{
	Watchdog dog{TIMEOUT, record};
	const auto end = xpum::watchdog::Clock::now() + 6 * TIMEOUT;
	while (xpum::watchdog::Clock::now() < end) {
		xpum::watchdog::keepalive();
		std::this_thread::sleep_for(TIMEOUT / 10);
	}
	CHECK(rec.snapshot().empty());
}

TEST_CASE_FIXTURE(Fixture, "a new stall after progress warns again")
{
	Watchdog dog{std::chrono::milliseconds{400}, record};
	REQUIRE(rec.pretimeout.try_acquire_for(WAIT));
	xpum::watchdog::keepalive();
	REQUIRE(rec.pretimeout.try_acquire_for(WAIT));
	REQUIRE(rec.timeout.try_acquire_for(WAIT));
	CHECK(rec.count(ExpiryKind::Pretimeout) == 2);
}

TEST_CASE_FIXTURE(Fixture, "sleepFor does not count as a stall")
{
	Watchdog dog{TIMEOUT, record};
	xpum::watchdog::sleepFor(3 * TIMEOUT);
	CHECK(rec.snapshot().empty());
	REQUIRE(rec.timeout.try_acquire_for(WAIT));
}

TEST_CASE_FIXTURE(Fixture, "sleepUntil does not count as a stall")
{
	Watchdog dog{TIMEOUT, record};
	xpum::watchdog::sleepUntil(xpum::watchdog::Clock::now() + 3 * TIMEOUT);
	CHECK(rec.snapshot().empty());
	REQUIRE(rec.timeout.try_acquire_for(WAIT));
}

TEST_CASE_FIXTURE(Fixture, "a suspended phase reports nothing, then a full timeout starts")
{
	Watchdog dog{TIMEOUT, record};
	std::chrono::steady_clock::time_point resumed;
	{
		const auto flashing = xpum::watchdog::suspend("firmware update");
		std::this_thread::sleep_for(5 * TIMEOUT);
		CHECK(rec.snapshot().empty());
		resumed = xpum::watchdog::Clock::now();
	}
	REQUIRE(rec.timeout.try_acquire_for(WAIT));
	CHECK(xpum::watchdog::Clock::now() - resumed >= TIMEOUT);

	const auto events = rec.snapshot();
	REQUIRE(events.size() == 2);
	CHECK(events[0].kind == ExpiryKind::Pretimeout);
	CHECK(events[1].kind == ExpiryKind::Timeout);
	CHECK(std::string_view{events[1].phase} == "startup");
}

namespace {

std::binary_semaphore timeoutEntered{0};
std::atomic<bool> timeoutReturned{false};

void slowTimeout(const Expiry &expiry) noexcept
{
	if (expiry.kind != ExpiryKind::Timeout) {
		return;
	}
	timeoutEntered.release();
	std::this_thread::sleep_for(5 * TIMEOUT);
	timeoutReturned.store(true);
}

} // namespace

TEST_CASE("a suspend that starts after the timeout is committed waits for the handler")
{
	Watchdog dog{TIMEOUT, slowTimeout};
	REQUIRE(timeoutEntered.try_acquire_for(WAIT));
	{
		const auto flashing = xpum::watchdog::suspend("firmware update");
		CHECK(timeoutReturned.load());
	}
}

TEST_CASE_FIXTURE(Fixture, "nested phases restore the outer name")
{
	Watchdog dog{TIMEOUT, record};
	const auto outer = xpum::watchdog::enter("command");
	{
		const auto inner = xpum::watchdog::enter("driver init");
	}
	REQUIRE(rec.timeout.try_acquire_for(WAIT));
	CHECK(std::string_view{rec.snapshot().back().phase} == "command");
}

TEST_CASE_FIXTURE(Fixture, "phases on two threads never leave a name that was not entered")
{
	Watchdog dog{TIMEOUT, record};
	{
		std::binary_semaphore entered{0};
		std::binary_semaphore leave{0};
		std::jthread worker{[&] {
			const auto phase = xpum::watchdog::enter("worker");
			entered.release();
			leave.acquire();
		}};
		entered.acquire();
		{
			const auto main = xpum::watchdog::enter("main");
		}
		leave.release();
	}
	REQUIRE(rec.timeout.try_acquire_for(WAIT));
	const std::string_view name{rec.snapshot().back().phase};
	// Out-of-order exits leave a stale outer name at worst, never a dangling pointer or the
	// name of a phase that is still being entered.
	CHECK((name == "startup" || name == "worker"));
}

TEST_CASE_FIXTURE(Fixture, "a zero timeout disables the watchdog")
{
	Watchdog dog{std::chrono::milliseconds::zero(), record};
	xpum::watchdog::keepalive();
	const auto phase = xpum::watchdog::enter("command");
	xpum::watchdog::sleepFor(3 * TIMEOUT);
	CHECK(rec.snapshot().empty());
}

TEST_CASE("free functions are no-ops when no watchdog is armed")
{
	xpum::watchdog::keepalive();
	const auto phase = xpum::watchdog::enter("command");
	const auto flashing = xpum::watchdog::suspend("firmware update");
	const auto start = xpum::watchdog::Clock::now();
	xpum::watchdog::sleepFor(10ms);
	xpum::watchdog::sleepUntil(xpum::watchdog::Clock::now() + 10ms);
	CHECK(xpum::watchdog::Clock::now() - start >= 20ms);
}

TEST_CASE_FIXTURE(Fixture, "a second watchdog stays inert")
{
	Watchdog first{60s, record};
	{
		Watchdog second{TIMEOUT, record};
		std::this_thread::sleep_for(3 * TIMEOUT);
	}
	CHECK(rec.snapshot().empty());
}

TEST_CASE_FIXTURE(Fixture, "destruction is prompt even with a long timeout")
{
	const auto start = xpum::watchdog::Clock::now();
	{
		Watchdog dog{60s, record};
		const auto phase = xpum::watchdog::enter("command");
	}
	CHECK(xpum::watchdog::Clock::now() - start < 100ms);
	CHECK(rec.snapshot().empty());
}

TEST_CASE("formatMessage")
{
	const Expiry pre{
		.kind = ExpiryKind::Pretimeout, .phase = "driver init", .timeout = 30s, .elapsed = 15s, .remaining = 15s};
	const Expiry timeout{
		.kind = ExpiryKind::Timeout, .phase = "driver init", .timeout = 30s, .elapsed = 30s, .remaining = 0ms};

	SUBCASE("pretimeout warning")
	{
		CHECK(format(pre, "stats") == "[Warning] xpu-smi: no progress for 15 s in phase 'driver init' while running "
									  "'stats'; aborting in 15 s unless it completes (see --timeout).\n");
	}
	SUBCASE("timeout")
	{
		CHECK(format(timeout, "stats") == "[Error] xpu-smi: no progress for 30 s in phase 'driver init' while running "
										  "'stats'; aborting (exit 124).\n");
	}
	SUBCASE("no command")
	{
		CHECK(format(timeout, "") ==
			  "[Error] xpu-smi: no progress for 30 s in phase 'driver init'; aborting (exit 124).\n");
	}
	SUBCASE("sub-second and fractional durations print in ms")
	{
		Expiry e = pre;
		e.elapsed = 500ms;
		e.remaining = 1500ms;
		CHECK(format(e, "") == "[Warning] xpu-smi: no progress for 500 ms in phase 'driver init'; aborting in 1500 ms "
							   "unless it completes (see --timeout).\n");
	}
	SUBCASE("truncation still ends the line")
	{
		std::array<char, 8> small{};
		const auto text = xpum::watchdog::formatMessage(timeout, "stats", small);
		CHECK(text == "[Error]\n");
	}
	SUBCASE("empty buffer") { CHECK(xpum::watchdog::formatMessage(timeout, "stats", std::span<char>{}).empty()); }
}
