/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "amc_update_interrupt.h"
#include "common.h"
#include <atomic>
#include <csignal>
#include <mutex>

namespace {

using signalHandler = void (*)(int);

// Only touched by the signal handler through lock-free atomics.
static_assert(std::atomic<bool>::is_always_lock_free, "cancel flag must be usable from a signal handler");
static_assert(std::atomic<int>::is_always_lock_free, "pending signal must be usable from a signal handler");
std::atomic<bool> gCancelRequested{false};
std::atomic<int> gPendingSignal{0};

// Guarded by gGuardMutex
std::mutex gGuardMutex;
unsigned int gActiveGuards = 0;
signalHandler gPrevSigint = SIG_DFL;
signalHandler gPrevSigterm = SIG_DFL;

/**
 * @brief SIGINT/SIGTERM handler installed while an AMC firmware update runs
 *
 * The first signal is held back and only raises the cancel request polled by the update flow; any
 * signal after it restores the default disposition and is raised again, ending the process at once.
 * Only touches lock-free atomics and async-signal-safe calls.
 *
 * @param sig The signal received
 */
extern "C" void amcUpdateSignalHandler(int sig)
{
	// Claim the pending slot atomically: when signals overlap (SIGINT and SIGTERM, or one per update
	// thread) exactly one of them is held back and every other one takes the forced exit below.
	int expected = 0;
	if (!gPendingSignal.compare_exchange_strong(expected, sig)) {
		// Second signal: the user does not want to wait for the cancel to finish.
		std::signal(sig, SIG_DFL);
		std::raise(sig);
		return;
	}
	gCancelRequested.store(true);
	// Some C runtimes (MSVC) reset the disposition to SIG_DFL before calling the handler.
	std::signal(sig, amcUpdateSignalHandler);
}

} // namespace

/**
 * @brief Start holding back SIGINT/SIGTERM for an AMC firmware update
 *
 * The first guard alive installs the handler and clears any earlier cancel request; guards created
 * while others are alive (parallel updates of several cards) share that state.
 */
AmcUpdateInterruptGuard::AmcUpdateInterruptGuard()
{
	std::lock_guard<std::mutex> lock(gGuardMutex);
	if (gActiveGuards++ > 0) {
		return;
	}

	gCancelRequested.store(false);
	gPendingSignal.store(0);
	gPrevSigint = std::signal(SIGINT, amcUpdateSignalHandler);
	gPrevSigterm = std::signal(SIGTERM, amcUpdateSignalHandler);
	if (gPrevSigint == SIG_ERR || gPrevSigterm == SIG_ERR) {
		ERR("Could not install the interrupt handler; interrupting the AMC update will not cancel it\n");
	}
}

/**
 * @brief Stop holding back SIGINT/SIGTERM once the last AMC firmware update has ended
 *
 * The last guard restores the handlers that were installed before, then raises the signal that was
 * held back, if any, so the process still ends the way the user asked for.
 */
AmcUpdateInterruptGuard::~AmcUpdateInterruptGuard()
{
	int pendingSignal = 0;
	{
		std::lock_guard<std::mutex> lock(gGuardMutex);
		if (--gActiveGuards > 0) {
			return;
		}

		std::signal(SIGINT, gPrevSigint == SIG_ERR ? SIG_DFL : gPrevSigint);
		std::signal(SIGTERM, gPrevSigterm == SIG_ERR ? SIG_DFL : gPrevSigterm);
		pendingSignal = gPendingSignal.exchange(0);
	}

	// Deliver the signal that was held back, now that every AMC update has been cancelled.
	if (pendingSignal != 0) {
		std::raise(pendingSignal);
	}
}

/**
 * @brief Whether the running AMC firmware updates have been asked to cancel
 *
 * @return bool True once SIGINT or SIGTERM has been received while a guard is alive
 */
bool AmcUpdateInterruptGuard::cancelRequested() { return gCancelRequested.load(); }
