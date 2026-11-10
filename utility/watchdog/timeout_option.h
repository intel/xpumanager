/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef UTILITY_WATCHDOG_TIMEOUT_OPTION_H
#define UTILITY_WATCHDOG_TIMEOUT_OPTION_H

#include "watchdog.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace xpum::watchdog {

struct TimeoutOption
{
	std::optional<std::chrono::seconds> timeout; // nullopt: not given, use defaultTimeout()
	std::vector<std::string> remaining;			 // argv without any --timeout occurrence; [0] kept
	std::optional<std::string> error;			 // set on a malformed value; timeout is then unspecified
};

/**
 * @brief Removes every `--timeout N` / `--timeout=N` from @p argv and parses the last one.
 *
 * Handled before the command-line parser runs so the option is accepted at any position,
 * before or after a subcommand. N is whole seconds in [0, MAX_TIMEOUT]; 0 disables the watchdog.
 *
 * @param[in] argv  Program arguments, argv[0] included.
 *
 * @retval TimeoutOption  The timeout (nullopt when absent) and the remaining arguments,
 *                        or `error` set when a value is missing or invalid.
 */
[[nodiscard]] TimeoutOption extractTimeoutOption(std::span<char *const> argv);

/**
 * @brief The timeout to arm the watchdog with, before the GPUs are counted.
 *
 * @param[in] option  Parsed --timeout option, without an error.
 *
 * @retval timeout  The given value (0 = watchdog off), else BASE_TIMEOUT.
 */
[[nodiscard]] constexpr std::chrono::seconds initialTimeout(const TimeoutOption &option) noexcept
{
	return option.timeout.value_or(BASE_TIMEOUT);
}

/**
 * @brief Lengthens @p dog to defaultTimeout() for the host's GPUs when no --timeout was given.
 *
 * The GPUs are counted while @p dog is armed with initialTimeout(), so a count that blocks
 * still ends in a timeout.
 *
 * @param[in,out] dog        The process watchdog, armed with initialTimeout(@p option).
 * @param[in]     option     Parsed --timeout option, without an error.
 * @param[in]     countGpus  Returns the host's GPU count; called only when no --timeout was given.
 */
template <typename CountGpus>
	requires std::is_invocable_r_v<std::size_t, CountGpus &>
void applyDefaultTimeout(Watchdog &dog, const TimeoutOption &option, CountGpus &&countGpus)
{
	if (option.timeout) {
		return;
	}
	const auto counting = enter("counting GPUs");
	dog.lengthenTimeout(defaultTimeout(std::invoke(countGpus)));
}

} // namespace xpum::watchdog

#endif // UTILITY_WATCHDOG_TIMEOUT_OPTION_H
