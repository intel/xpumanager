/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "circuit_breaker.h"

#include <atomic>
#include <chrono>
#include <concepts>
#include <future>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef __linux__
#include <filesystem>
#endif

using namespace std::chrono_literals;
using xpum::BreakerEvent;
using xpum::CircuitBreaker;

namespace {

constexpr auto SHORT_LIMIT = std::chrono::milliseconds{100};
constexpr auto LONG_LIMIT = std::chrono::milliseconds{5000};

/// Blocks the calls that wait on it until released, so an abandoned call ends with the test.
class Gate
{
public:
	Gate() : opened(released.get_future().share()) {}
	Gate(const Gate &) = delete;
	Gate &operator=(const Gate &) = delete;
	Gate(Gate &&) = delete;
	Gate &operator=(Gate &&) = delete;
	~Gate() { release(); }

	[[nodiscard]] std::shared_future<void> waiter() const { return opened; }
	void release()
	{
		if (!isReleased.exchange(true)) {
			released.set_value();
		}
	}

private:
	std::promise<void> released;
	std::shared_future<void> opened;
	std::atomic<bool> isReleased{false};
};

/// What the observer was told, in order. The observer is a plain function, so this is global.
std::mutex eventsMutex;
std::vector<BreakerEvent> events;

void recordEvent(std::string_view, std::chrono::milliseconds, BreakerEvent event) noexcept
{
	const std::lock_guard lock{eventsMutex};
	events.push_back(event);
}

[[nodiscard]] std::vector<BreakerEvent> takeEvents()
{
	const std::lock_guard lock{eventsMutex};
	return std::exchange(events, {});
}

/// Polls @p condition for up to @p within; the worker resets the breaker asynchronously.
[[nodiscard]] bool eventually(std::predicate auto condition, std::chrono::milliseconds within)
{
	const auto until = std::chrono::steady_clock::now() + within;
	while (!condition()) {
		if (std::chrono::steady_clock::now() > until) {
			return false;
		}
		std::this_thread::sleep_for(5ms);
	}
	return true;
}

} // namespace

TEST_CASE("a call that finishes in time returns its result")
{
	CircuitBreaker breaker{"test", LONG_LIMIT};
	CHECK(breaker.run([] { return 42; }) == 42);
	CHECK_FALSE(breaker.isOpen());
}

TEST_CASE("move-only calls and results are supported")
{
	CircuitBreaker breaker{"test", LONG_LIMIT};
	auto value = std::make_unique<int>(7);
	auto result = breaker.run([owned = std::move(value)]() mutable { return std::move(owned); });
	REQUIRE(result.has_value());
	CHECK(**result == 7);
}

TEST_CASE("an exception thrown by the call is rethrown to the caller")
{
	CircuitBreaker breaker{"test", LONG_LIMIT};
	CHECK_THROWS_AS((void)breaker.run([]() -> int { throw std::runtime_error("boom"); }), std::runtime_error);
	CHECK_FALSE(breaker.isOpen());
}

TEST_CASE("every call of a breaker runs on the same worker thread, one at a time")
{
	CircuitBreaker breaker{"test", LONG_LIMIT};
	const auto first = breaker.run([] { return std::this_thread::get_id(); });
	const auto second = breaker.run([] { return std::this_thread::get_id(); });
	REQUIRE(first.has_value());
	CHECK(first == second);
	CHECK(*first != std::this_thread::get_id());
}

TEST_CASE("a call that misses the limit trips the breaker open, which resets when the call returns")
{
	(void)takeEvents();
	Gate gate;
	CircuitBreaker breaker{"RAS", SHORT_LIMIT, recordEvent};

	CHECK_FALSE(breaker
					.run([waiter = gate.waiter()] {
						waiter.wait();
						return 1;
					})
					.has_value());
	CHECK(breaker.isOpen());
	CHECK(CircuitBreaker::anyAbandoned());

	// Calls in the same group would block the same way, so they are no longer made.
	auto ran = std::make_shared<std::atomic<bool>>(false);
	CHECK_FALSE(breaker
					.run([ran] {
						ran->store(true);
						return 2;
					})
					.has_value());
	CHECK_FALSE(ran->load());

	gate.release();
	CHECK(eventually([&] { return !breaker.isOpen(); }, LONG_LIMIT));
	CHECK_FALSE(CircuitBreaker::anyAbandoned());
	CHECK(breaker.run([] { return 3; }) == 3);
	CHECK(takeEvents() == std::vector{BreakerEvent::Tripped, BreakerEvent::Reset});
}

TEST_CASE("the limit counts from the start of a call, not from when it was queued")
{
	// Each call takes 60% of the limit, so the second, queued behind the first, would miss a limit
	// counted from when it was queued. The other 40% absorbs slow thread wake-ups, as under
	// Valgrind, which runs one thread at a time.
	constexpr auto limit = std::chrono::milliseconds{1000};
	CircuitBreaker breaker{"test", limit};
	const auto slow = [callTime = limit * 6 / 10] {
		std::this_thread::sleep_for(callTime);
		return true;
	};
	std::optional<bool> first;
	std::thread other{[&] { first = breaker.run(slow); }};
	const auto second = breaker.run(slow);
	other.join();
	CHECK(first == true);
	CHECK(second == true);
	CHECK_FALSE(breaker.isOpen());
}

TEST_CASE("calls queued behind an abandoned call are dropped without running")
{
	Gate gate;
	CircuitBreaker breaker{"test", SHORT_LIMIT};
	std::optional<int> blocked;
	std::thread first{[&] {
		blocked = breaker.run([waiter = gate.waiter()] {
			waiter.wait();
			return 1;
		});
	}};
	// Queue a second call behind the blocked one before it misses the limit.
	std::this_thread::sleep_for(SHORT_LIMIT / 4);
	auto ran = std::make_shared<std::atomic<bool>>(false);
	const auto queued = breaker.run([ran] {
		ran->store(true);
		return 2;
	});
	first.join();
	CHECK_FALSE(blocked.has_value());
	CHECK_FALSE(queued.has_value());
	CHECK_FALSE(ran->load());
	gate.release();
}

TEST_CASE("separate breakers trip separately")
{
	Gate gate;
	CircuitBreaker stuck{"stuck", SHORT_LIMIT};
	CircuitBreaker healthy{"healthy", LONG_LIMIT};
	CHECK_FALSE(stuck
					.run([waiter = gate.waiter()] {
						waiter.wait();
						return 0;
					})
					.has_value());
	CHECK(stuck.isOpen());
	CHECK(healthy.run([] { return 3; }) == 3);
	CHECK_FALSE(healthy.isOpen());
	gate.release();
}

TEST_CASE("destroying a breaker does not wait for a call that never returns")
{
	Gate gate;
	const auto start = std::chrono::steady_clock::now();
	{
		CircuitBreaker breaker{"test", SHORT_LIMIT};
		CHECK_FALSE(breaker
						.run([waiter = gate.waiter()] {
							waiter.wait();
							return 0;
						})
						.has_value());
	}
	CHECK(std::chrono::steady_clock::now() - start < LONG_LIMIT);
	gate.release();
}

TEST_CASE("a call that finished keeps its result when the next call trips the breaker")
{
	// The quick call runs first and finishes while the next call is queued; its caller may not run
	// again until after the next call has tripped the breaker. Its answer must not be taken for a
	// dropped call. Repeated, since when the caller runs again cannot be forced. The limit leaves
	// the quick call, which waits for the main thread, ample time even under Valgrind; only the
	// blocked call can miss it.
	constexpr int rounds = 5;
	constexpr auto limit = std::chrono::milliseconds{250};
	for (int round = 0; round < rounds; ++round) {
		Gate blocked;
		Gate go;
		std::promise<void> quickStarted;
		CircuitBreaker breaker{"test", limit};
		std::optional<int> quick;
		std::thread first{[&] {
			quick = breaker.run([&quickStarted, waiter = go.waiter()] {
				quickStarted.set_value();
				waiter.wait();
				return 1;
			});
		}};
		quickStarted.get_future().wait();
		std::optional<int> slow;
		std::thread second{[&] {
			slow = breaker.run([waiter = blocked.waiter()] {
				waiter.wait();
				return 2;
			});
		}};
		// Let the slow call queue behind the quick one, then let the quick one finish.
		std::this_thread::sleep_for(1ms);
		go.release();
		second.join();
		first.join();
		CHECK(quick == 1);
		CHECK_FALSE(slow.has_value());
		blocked.release();
	}
}

#ifdef __linux__
namespace {

[[nodiscard]] std::size_t threadCount()
{
	const std::filesystem::directory_iterator tasks{"/proc/self/task"};
	return static_cast<std::size_t>(std::distance(begin(tasks), end(tasks)));
}

} // namespace

TEST_CASE("a worker left inside a call ends once the call returns")
{
	const std::size_t before = threadCount();
	Gate gate;
	{
		CircuitBreaker breaker{"test", SHORT_LIMIT};
		CHECK_FALSE(breaker
						.run([waiter = gate.waiter()] {
							waiter.wait();
							return 0;
						})
						.has_value());
	}
	gate.release();
	CHECK(eventually([&] { return threadCount() <= before; }, LONG_LIMIT));
}
#endif
