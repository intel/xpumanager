/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef LOGGER_SINK_BASE_H
#define LOGGER_SINK_BASE_H

#include "log_level.h"
#include "log_record.h"

#include <chrono>
#include <cstdio>
#include <exception>
#include <mutex>
#include <source_location>
#include <string_view>
#include <thread>

/**
 * @brief Abstract output sink — the extensibility point of the logger.
 *
 * Derive from this to direct log output anywhere: files, network sockets,
 * in-memory buffers, structured JSON streams, etc.
 *
 * The interface mirrors the sync-based design used by standard logging
 * libraries (e.g. std::basic_streambuf, spdlog::sinks::sink):
 *   - emit()  — receive a formatted message for a single log event
 *   - sync()  — flush any internally buffered bytes (equivalent to
 *               std::streambuf::sync(), called after every emit())
 */
class Sink
{
	std::mutex mutex;

public:
	Sink() = default;
	Sink(const Sink &) = delete;
	Sink &operator=(const Sink &) = delete;
	Sink(Sink &&) = delete;
	Sink &operator=(Sink &&) = delete;
	virtual ~Sink() = default;

	/**
	 * Thread-safe entry point called by Logger. Serializes emit() + sync()
	 * under a per-sink mutex so concurrent log calls from multiple threads
	 * produce coherent, non-interleaved output. Independent sinks never
	 * contend with each other.
	 *
	 * Virtual so sinks with their own thread-safety model (e.g. AsyncSink)
	 * can override and bypass this mutex entirely.
	 *
	 * If emit() or sync() throw, the exception is caught here and written
	 * to stderr directly — Logger cannot be used from inside a sink.
	 */
	virtual void log(LogLevel level, std::source_location loc, std::string_view prefix, std::string_view msg) noexcept
	{
		try {
			LogRecord const record{
				level, loc, prefix, msg, std::chrono::system_clock::now(), std::this_thread::get_id()};
			std::lock_guard const lock(mutex);
			emit(record);
			sync();
		} catch (const std::exception &e) {
			std::fprintf(stderr, "[logger sink error] %s\n", e.what());
		} catch (...) {
			std::fprintf(stderr, "[logger sink error] unknown exception\n");
		}
	}

	/// Override to write a formatted log record.  May throw; exceptions are
	/// caught by log() and reported to stderr.
	virtual void emit(const LogRecord &record) = 0;

	/// Override to flush internal buffers after emit().  May throw; exceptions
	/// are caught by log() and reported to stderr.
	virtual void sync() = 0;
};

#endif /* LOGGER_SINK_BASE_H */
