/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifdef INFO
#undef INFO
#endif

#include "bounded_call.h"

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

using namespace std::chrono_literals;

namespace {

constexpr auto SHORT_LIMIT = std::chrono::milliseconds{100};
constexpr auto LONG_LIMIT = std::chrono::milliseconds{5000};

/// Shaped like a Level Zero output struct.
struct Reading
{
	int stype = 0;
	void *pNext = nullptr;
	int value = 0;
};

ze_result_t fillReading(int handle, Reading *out)
{
	out->value = handle * 10 + out->stype;
	return ZE_RESULT_SUCCESS;
}

ze_result_t failReading(int handle, Reading *out)
{
	out->value = handle;
	return ZE_RESULT_ERROR_DEVICE_LOST;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): a plain getter cannot capture
std::promise<void> releaseBlocked;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::shared_future<void> blockedUntil = releaseBlocked.get_future().share();

ze_result_t blockReading(int handle, Reading *out)
{
	blockedUntil.wait();
	out->value = handle;
	return ZE_RESULT_SUCCESS;
}

} // namespace

TEST_CASE("boundedGet passes the arguments and the caller's stype, and writes the answer back")
{
	xpum::CircuitBreaker breaker{"test", LONG_LIMIT};
	Reading reading{.stype = 3, .pNext = nullptr, .value = 0};
	CHECK(boundedGet<fillReading>(breaker, &reading, 4) == ZE_RESULT_SUCCESS);
	CHECK(reading.value == 43);
}

TEST_CASE("boundedGet leaves the output unchanged when the getter fails")
{
	xpum::CircuitBreaker breaker{"test", LONG_LIMIT};
	Reading reading{.stype = 0, .pNext = nullptr, .value = 7};
	CHECK(boundedGet<failReading>(breaker, &reading, 4) == ZE_RESULT_ERROR_DEVICE_LOST);
	CHECK(reading.value == 7);
}

TEST_CASE("boundedGet refuses a null output and an extension chain the call would write through")
{
	xpum::CircuitBreaker breaker{"test", LONG_LIMIT};
	CHECK(boundedGet<fillReading>(breaker, static_cast<Reading *>(nullptr), 1) == ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	int extension = 0;
	Reading chained{.stype = 0, .pNext = &extension, .value = 7};
	CHECK(boundedGet<fillReading>(breaker, &chained, 1) == ZE_RESULT_ERROR_INVALID_ARGUMENT);
	CHECK(chained.value == 7);
}

TEST_CASE("a getter that does not return reads CALL_ABANDONED and never writes the caller's output")
{
	xpum::CircuitBreaker breaker{"test", SHORT_LIMIT};
	Reading reading{.stype = 0, .pNext = nullptr, .value = 7};
	CHECK(boundedGet<blockReading>(breaker, &reading, 5) == CALL_ABANDONED);
	CHECK(reading.value == 7);
	// The group's other calls are not made while the abandoned one is running.
	CHECK(boundedGet<fillReading>(breaker, &reading, 1) == CALL_ABANDONED);

	releaseBlocked.set_value();
	const auto until = std::chrono::steady_clock::now() + LONG_LIMIT;
	while (breaker.isOpen() && std::chrono::steady_clock::now() < until) {
		std::this_thread::sleep_for(5ms);
	}
	CHECK(reading.value == 7);
	CHECK(boundedGet<fillReading>(breaker, &reading, 1) == ZE_RESULT_SUCCESS);
	CHECK(reading.value == 10);
	CHECK_FALSE(boundedCallsAbandoned());
}

TEST_CASE("boundedCall gives a struct result its status when the call is abandoned")
{
	struct Answer
	{
		ze_result_t status{};
		int value = 0;
	};
	xpum::CircuitBreaker breaker{"test", SHORT_LIMIT};
	std::promise<void> release;
	auto answer = boundedCall(breaker, [until = release.get_future().share()] {
		until.wait();
		return Answer{ZE_RESULT_SUCCESS, 1};
	});
	CHECK(answer.status == CALL_ABANDONED);
	CHECK(answer.value == 0);
	release.set_value();
}
