/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef LOGGER_OSTREAM_SINK_H
#define LOGGER_OSTREAM_SINK_H

#include "log_record.h"
#include "log_level.h"
#include "sink_base.h"

#include <chrono>
#include <format>
#include <ostream>

/**
 * @brief Sink that writes to any std::ostream (std::cout, std::cerr, etc.).
 *
 * Does not own the stream — the stream lifetime must exceed that of this sink.
 * Use std::cout for stdout, std::cerr for stderr.
 * Auto-newline: a '\n' is appended unless the message already ends with
 * '\n' or contains a bare '\r' (carriage-return without newline), which
 * signals the caller is managing cursor position for in-place updates such
 * as progress bars.  In that case the message is emitted verbatim.
 */
class OStreamSink final : public Sink
{
	std::ostream &os;
	bool mShowTimestamp;
	bool mShowThreadId;

public:
	explicit OStreamSink(std::ostream &stream, bool showTimestamp = false, bool showThreadId = false) noexcept
		: os(stream), mShowTimestamp(showTimestamp), mShowThreadId(showThreadId)
	{}

	void emit(const LogRecord &record) override
	{
		if (mShowTimestamp) {
			os << std::format("[{:%H:%M:%S}] ", std::chrono::floor<std::chrono::milliseconds>(record.timestamp));
		}
		if (mShowThreadId) {
			os << '[' << record.threadId << "] ";
		}
		if (record.level == LogLevel::DBG) {
			os << record.prefix << record.loc.file_name() << ':' << record.loc.line() << ": ";
		} else if (!record.prefix.empty()) {
			os << record.prefix;
		}
		os << record.msg;
		// Don't add a newline if the message ends with '\n' already, or if
		// it contains '\r' (bare CR = in-place progress update; caller owns
		// the cursor/line positioning).
		const bool callerManagesLine =
			!record.msg.empty() && (record.msg.back() == '\n' || record.msg.find('\r') != std::string_view::npos);
		if (!callerManagesLine) {
			os << '\n';
		}
	}

	void sync() noexcept override { os.flush(); }
};

#endif /* LOGGER_OSTREAM_SINK_H */
