/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef UTILITY_WATCHDOG_TIMEOUT_OPTION_H
#define UTILITY_WATCHDOG_TIMEOUT_OPTION_H

#include "watchdog.h"

#include <chrono>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace xpum::watchdog {

struct TimeoutOption
{
	std::chrono::seconds timeout{DEFAULT_TIMEOUT};
	std::vector<std::string> remaining; // argv without any --timeout occurrence; [0] kept
	std::optional<std::string> error;	// set on a malformed value; timeout is then unspecified
};

/**
 * @brief Removes every `--timeout N` / `--timeout=N` from @p argv and parses the last one.
 *
 * Handled before the command-line parser runs so the option is accepted at any position,
 * before or after a subcommand. N is whole seconds in [0, MAX_TIMEOUT]; 0 disables the watchdog.
 *
 * @param[in] argv  Program arguments, argv[0] included.
 *
 * @retval TimeoutOption  The timeout (DEFAULT_TIMEOUT when absent) and the remaining arguments,
 *                        or `error` set when a value is missing or invalid.
 */
[[nodiscard]] TimeoutOption extractTimeoutOption(std::span<char *const> argv);

} // namespace xpum::watchdog

#endif // UTILITY_WATCHDOG_TIMEOUT_OPTION_H
