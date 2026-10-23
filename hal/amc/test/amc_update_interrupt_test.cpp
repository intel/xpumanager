/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "amc_update_interrupt.h"
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

namespace {

volatile std::sig_atomic_t gOuterHandlerCalls = 0;
volatile std::sig_atomic_t gOuterHandlerSignal = 0;

/**
 * @brief Signal handler standing in for the process' own one; counts the signals it receives
 *
 * @param sig The signal received
 */
void outerHandler(int sig)
{
	gOuterHandlerCalls = gOuterHandlerCalls + 1;
	gOuterHandlerSignal = sig;
}

/**
 * @brief Stand-in for the process' own disposition, so a re-raised signal can be observed instead of killing the test
 */
struct OuterHandler
{
	/**
	 * @brief Install outerHandler() for SIGINT and SIGTERM and clear its counters
	 */
	OuterHandler()
	{
		gOuterHandlerCalls = 0;
		gOuterHandlerSignal = 0;
		std::signal(SIGINT, outerHandler);
		std::signal(SIGTERM, outerHandler);
	}
	/**
	 * @brief Restore the default disposition of SIGINT and SIGTERM
	 */
	~OuterHandler()
	{
		std::signal(SIGINT, SIG_DFL);
		std::signal(SIGTERM, SIG_DFL);
	}
};

} // namespace

TEST_CASE("Without a signal nothing is requested or re-raised")
{
	OuterHandler outer;
	{
		AmcUpdateInterruptGuard guard;
		CHECK_FALSE(AmcUpdateInterruptGuard::cancelRequested());
	}
	CHECK(gOuterHandlerCalls == 0);
	// The previous handler is back in place
	std::raise(SIGINT);
	CHECK(gOuterHandlerCalls == 1);
}

TEST_CASE("A signal is held back until the last guard ends, then delivered")
{
	OuterHandler outer;
	{
		AmcUpdateInterruptGuard first;
		{
			AmcUpdateInterruptGuard second;
			std::raise(SIGTERM);
			CHECK(AmcUpdateInterruptGuard::cancelRequested());
			CHECK(gOuterHandlerCalls == 0);
		}
		// One guard still alive: still held back
		CHECK(gOuterHandlerCalls == 0);
	}
	CHECK(gOuterHandlerCalls == 1);
	CHECK(gOuterHandlerSignal == SIGTERM);
}

TEST_CASE("A new update starts without a stale cancel request")
{
	OuterHandler outer;
	{
		AmcUpdateInterruptGuard guard;
		std::raise(SIGINT);
	}
	AmcUpdateInterruptGuard guard;
	CHECK_FALSE(AmcUpdateInterruptGuard::cancelRequested());
}

TEST_CASE("A second signal ends the process without waiting for the cancel")
{
	const pid_t pid = fork();
	REQUIRE(pid >= 0);
	if (pid == 0) {
		AmcUpdateInterruptGuard guard;
		std::raise(SIGINT);
		std::raise(SIGINT);
		_exit(0); // not reached when the second signal terminates the process
	}

	int status = 0;
	REQUIRE(waitpid(pid, &status, 0) == pid);
	CHECK(WIFSIGNALED(status));
	CHECK(WTERMSIG(status) == SIGINT);
}

TEST_CASE("A different signal after a held one also ends the process")
{
	const pid_t pid = fork();
	REQUIRE(pid >= 0);
	if (pid == 0) {
		AmcUpdateInterruptGuard guard;
		std::raise(SIGINT);
		std::raise(SIGTERM);
		_exit(0); // not reached when SIGTERM terminates the process
	}

	int status = 0;
	REQUIRE(waitpid(pid, &status, 0) == pid);
	CHECK(WIFSIGNALED(status));
	CHECK(WTERMSIG(status) == SIGTERM);
}
