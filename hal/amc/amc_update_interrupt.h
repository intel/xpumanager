/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef __AMC_UPDATE_INTERRUPT_H
#define __AMC_UPDATE_INTERRUPT_H

/**
 * @brief Holds back SIGINT/SIGTERM while AMC firmware updates are running
 *
 * Killing xpu-smi in the middle of an update would leave the firmware device in update mode.
 * While at least one guard is alive, the first SIGINT or SIGTERM only records a cancel request,
 * which the PLDM update flow polls through cancelRequested() so it can send CancelUpdate
 * (DSP0267 section 12.14) first. A second signal terminates the process straight away.
 *
 * When the last guard goes out of scope the previous handlers are restored and a signal that
 * was held back is raised again, so the process still ends the way the user asked for.
 * Guards may be alive in several threads at once (parallel updates of several cards).
 */
class AmcUpdateInterruptGuard
{
public:
	AmcUpdateInterruptGuard();
	~AmcUpdateInterruptGuard();

	AmcUpdateInterruptGuard(const AmcUpdateInterruptGuard &) = delete;
	AmcUpdateInterruptGuard &operator=(const AmcUpdateInterruptGuard &) = delete;
	AmcUpdateInterruptGuard(AmcUpdateInterruptGuard &&) = delete;
	AmcUpdateInterruptGuard &operator=(AmcUpdateInterruptGuard &&) = delete;

	// True once SIGINT or SIGTERM has been received while a guard is alive
	static bool cancelRequested();
};

#endif // __AMC_UPDATE_INTERRUPT_H
