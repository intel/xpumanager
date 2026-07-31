/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef LOGGER_LOG_RECORD_H
#define LOGGER_LOG_RECORD_H

#include "log_level.h"

#include <chrono>
#include <source_location>
#include <string_view>
#include <thread>

/// Bundles all metadata for a single log event.  Passed to Sink::emit() so
/// every sink has structured access to level, location, timestamp, and thread.
struct LogRecord
{
	LogLevel level;
	std::source_location loc;
	std::string_view prefix;
	std::string_view msg;
	std::chrono::system_clock::time_point timestamp;
	std::thread::id threadId;
};

#endif /* LOGGER_LOG_RECORD_H */
