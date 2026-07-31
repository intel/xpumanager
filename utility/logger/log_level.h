/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef LOGGER_LOG_LEVEL_H
#define LOGGER_LOG_LEVEL_H

#include <functional>
#include <string_view>

/**
 * Log levels.  Pass to setDbgLvl() or any level parameter:
 *   LogLevel::NO_PRINT (-1)  suppress all output
 *   LogLevel::ERR      ( 0)  errors only
 *   LogLevel::INFO     ( 1)  + informational messages
 *   LogLevel::DBG      ( 2)  + debug messages
 *   LogLevel::TRACE    ( 3)  + function entry/exit tracing
 */
enum class LogLevel : int
{
	NO_PRINT = -1,
	ERR,
	INFO,
	DBG,
	TRACE,
};

constexpr auto operator<=>(LogLevel a, LogLevel b) noexcept { return static_cast<int>(a) <=> static_cast<int>(b); }

/// Callback invoked when a format or sink error is encountered.
/// Must be fast and must NOT call Logger — re-entrant use is undefined.
using LogErrorCallback = std::function<void(std::string_view what)>;

/// Controls what happens when std::vformat throws (e.g. a bad format string).
enum class FormatErrorPolicy
{
	Report,		///< Call invokeOnError() with the error description (default).
	Substitute, ///< Emit "[format error: <what>]" in place of the message.
	Ignore,		///< Silently discard the message.
	Terminate,	///< Call std::terminate() — useful in debug/test builds.
};

#endif /* LOGGER_LOG_LEVEL_H */
