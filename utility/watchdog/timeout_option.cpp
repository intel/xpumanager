/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "timeout_option.h"

#include <charconv>
#include <cstdint>
#include <string_view>
#include <system_error>

namespace xpum::watchdog {

namespace {

constexpr std::string_view OPTION = "--timeout";

/**
 * @brief Parses a --timeout value: whole seconds from 0 to MAX_TIMEOUT.
 *
 * @param[in] text  The value as given.
 *
 * @retval seconds       The timeout; 0 turns the watchdog off.
 * @retval std::nullopt  @p text is empty, not a whole number, or out of range.
 */
[[nodiscard]] std::optional<std::chrono::seconds> parseSeconds(std::string_view text) noexcept
{
	std::int64_t value = 0;
	const char *end = text.data() + text.size();
	const auto [ptr, ec] = std::from_chars(text.data(), end, value);
	if (text.empty() || ec != std::errc{} || ptr != end || value < 0 || value > MAX_TIMEOUT.count()) {
		return std::nullopt;
	}
	return std::chrono::seconds{value};
}

/**
 * @brief Builds the error message for an invalid --timeout value.
 *
 * @param[in] text  The value as given.
 *
 * @retval The message, naming the valid range and @p text.
 */
[[nodiscard]] std::string invalidValue(std::string_view text)
{
	std::string message{"--timeout expects whole seconds between 0 and "};
	message += std::to_string(MAX_TIMEOUT.count());
	message += ", got '";
	message += text;
	message += "'";
	return message;
}

} // namespace

/**
 * @brief Removes every `--timeout N` / `--timeout=N` from @p argv and parses the last one.
 *
 * @param[in] argv  Program arguments, argv[0] included.
 *
 * @retval TimeoutOption  The timeout (DEFAULT_TIMEOUT when absent) and the remaining arguments,
 *                        or `error` set when a value is missing or invalid.
 */
TimeoutOption extractTimeoutOption(std::span<char *const> argv)
{
	TimeoutOption result;
	result.remaining.reserve(argv.size());
	for (std::size_t i = 0; i < argv.size(); ++i) {
		const std::string_view arg{argv[i]};
		if (i == 0 || !arg.starts_with(OPTION)) {
			result.remaining.emplace_back(arg);
			continue;
		}
		std::string_view value;
		if (arg == OPTION) {
			if (i + 1 == argv.size()) {
				result.error = "--timeout requires a value in seconds";
				return result;
			}
			value = argv[++i];
		} else if (arg[OPTION.size()] == '=') {
			value = arg.substr(OPTION.size() + 1);
		} else {
			// Some other option that merely starts with "--timeout".
			result.remaining.emplace_back(arg);
			continue;
		}
		const auto seconds = parseSeconds(value);
		if (!seconds) {
			result.error = invalidValue(value);
			return result;
		}
		result.timeout = *seconds;
	}
	return result;
}

} // namespace xpum::watchdog
