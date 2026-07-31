/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef LOGGER_DEFERRED_SINK_H
#define LOGGER_DEFERRED_SINK_H

#include "log_level.h"
#include "log_record.h"
#include "ostream_sink.h"
#include "sink_base.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <source_location>
#include <string>
#include <vector>

/**
 * @brief Sink that buffers all log records in memory and emits them on destruction.
 *
 * Records are flushed to a backend sink (default: @c std::cerr) when the
 * @c DeferredSink is destroyed — which, when it is installed as the Logger's
 * log sink, happens at static-destruction time after @c main() returns.
 *
 * This guarantees that all @c PRINT output (which goes through the separate
 * @c printSink → stdout) appears before any diagnostic log output.
 *
 * Ordering: records are emitted in the exact order they were logged.
 * Each call to @c emit() runs under @c Sink::log()'s per-sink mutex, so
 * concurrent producers never reorder entries in the buffer.
 *
 * @note If the process terminates abnormally (@c std::terminate, @c abort,
 *       signals), buffered records are lost.  Call @c flush() explicitly from
 *       a signal/termination handler if this matters.
 *
 * @code
 *     // In main(), after Logger is otherwise configured:
 *     Logger::instance().setLogSink(std::make_shared<DeferredSink>());
 * @endcode
 */
class DeferredSink final : public Sink
{
	// Owns its strings so records are valid for the lifetime of the buffer.
	// LogRecord uses string_views that reference caller temporaries — unsafe
	// to store directly.
	struct LogEvent
	{
		LogLevel level;
		std::source_location loc;
		std::string prefix;
		std::string message;
		std::chrono::system_clock::time_point timestamp;
		std::thread::id threadId;

		explicit LogEvent(const LogRecord &r)
			: level(r.level), loc(r.loc), prefix(r.prefix), message(r.msg), timestamp(r.timestamp), threadId(r.threadId)
		{}
	};

	std::vector<LogEvent> mEvents;
	std::shared_ptr<Sink> mBackend;

public:
	/**
	 * @param backend  Where to write records on flush.  Defaults to
	 *                 @c OStreamSink(std::cerr).  Must not be null.
	 */
	explicit DeferredSink(std::shared_ptr<Sink> backend = std::make_shared<OStreamSink>(std::cerr)) noexcept
		: mBackend(backend ? std::move(backend) : std::make_shared<OStreamSink>(std::cerr))
	{}

	// Copying would cause both objects to flush the same events to the same backend on destruction.
	DeferredSink(const DeferredSink &) = delete;
	DeferredSink &operator=(const DeferredSink &) = delete;

	// Sink deletes move (std::mutex is non-movable); always use via shared_ptr.
	DeferredSink(DeferredSink &&) = delete;
	DeferredSink &operator=(DeferredSink &&) = delete;

	/**
	 * @brief Flush all buffered records to the backend in registration order.
	 *
	 * Called implicitly by the destructor.  May also be called explicitly,
	 * e.g. from a signal or termination handler.  Not thread-safe — call only
	 * when no other thread may be logging.
	 */
	void flush()
	{
		for (const auto &ev : mEvents) {
			mBackend->log(ev.level, ev.loc, ev.prefix, ev.message);
		}
		mEvents.clear();
	}

	~DeferredSink() override { flush(); }

	void emit(const LogRecord &record) override { mEvents.emplace_back(record); }

	/// No-op: the buffer is not flushed until destruction (or explicit flush()).
	void sync() noexcept override {}
};

#endif /* LOGGER_DEFERRED_SINK_H */
