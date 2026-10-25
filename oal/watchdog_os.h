/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef OAL_WATCHDOG_OS_H
#define OAL_WATCHDOG_OS_H

#include <string_view>

/*
 * OS actions for the hang watchdog. Every function here may run while another thread is stuck
 * in the driver holding the heap, stdio or logger locks, so none of them allocate or lock.
 * Output is best effort and bounded: a write that cannot complete in a short time, because the
 * reader of a pipe stopped or a terminal is stopped, is dropped, together with every later write
 * to that stream, so the caller always reaches its exit.
 */

/**
 * @brief Writes @p text to stdout, unbuffered.
 * @param[in] text  Bytes to write; a failed write is dropped.
 */
void writeStdoutRaw(std::string_view text) noexcept;

/**
 * @brief Writes @p text to stderr, unbuffered.
 * @param[in] text  Bytes to write; a failed write is dropped.
 */
void writeStderrRaw(std::string_view text) noexcept;

/**
 * @brief Best effort: writes one line per thread of this process to stderr.
 *
 * Linux adds each thread's scheduler state and kernel wait channel, which shows where in the
 * driver a hung thread is blocked.
 */
void writeThreadReport() noexcept;

#endif // OAL_WATCHDOG_OS_H
